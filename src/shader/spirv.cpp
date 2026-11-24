#include "scale/shader/spirv.h"

#include <format>
#include <initializer_list>
#include <span>
#include <string_view>
#include <utility>

namespace scale::shader {
namespace {

using Word = std::uint32_t;
using Words = std::vector<Word>;

// Encoding values from the Khronos SPIR-V unified specification and grammar.
// Keeping this subset local lets the compiler build without the Vulkan SDK.
enum class Op : std::uint16_t {
    MemoryModel = 14, EntryPoint = 15, ExecutionMode = 16, Capability = 17,
    TypeVoid = 19, TypeBool = 20, TypeInt = 21, TypeFloat = 22, TypeArray = 28,
    TypeStruct = 30, TypePointer = 32, TypeFunction = 33,
    ConstantTrue = 41, ConstantFalse = 42, Constant = 43,
    Function = 54, FunctionEnd = 56, Variable = 59, Load = 61, Store = 62,
    AccessChain = 65, Decorate = 71, MemberDecorate = 72, CopyObject = 83,
    Bitcast = 124, IAdd = 128, FAdd = 129, IMul = 132, FMul = 133, UDiv = 134,
    ULessThan = 176, Label = 248, Return = 253,
};

constexpr Word kMagic = 0x07230203;
constexpr Word kVersion = 0x00010500;
constexpr Word kMaxIdBound = 0x003fffff;
constexpr Word kShader = 1;
constexpr Word kRteCapability = 4467;
constexpr Word kLogical = 0;
constexpr Word kGlsl450 = 1;
constexpr Word kCompute = 5;
constexpr Word kLocalSize = 17;
constexpr Word kRteMode = 4462;
constexpr Word kInput = 1;
constexpr Word kStorageBuffer = 12;
constexpr Word kBlock = 2;
constexpr Word kArrayStride = 6;
constexpr Word kBuiltIn = 11;
constexpr Word kNonWritable = 24;
constexpr Word kNonReadable = 25;
constexpr Word kBinding = 33;
constexpr Word kDescriptorSet = 34;
constexpr Word kOffset = 35;
constexpr Word kNoContraction = 42;
constexpr Word kLocalInvocationIndex = 29;

constexpr Word kWaveSize = 32;
constexpr Word kWordBytes = 4;
constexpr IrValueType kLaneWord{IrType::u32, IrLanes::per_lane};
constexpr IrValueType kUniformFlag{IrType::flag, IrLanes::uniform};

void write(Words& section, Op opcode, std::initializer_list<Word> operands) {
    section.push_back((static_cast<Word>(operands.size() + 1) << 16) | static_cast<Word>(opcode));
    section.insert(section.end(), operands);
}

class Emitter {
public:
    explicit Emitter(const IrProgram& program)
        : program_(program), ids_(program.instructions.size()), scc_(program.entry_scc) {}

    Result<SpirvModule> run() {
        if (program_.instructions.size() < 2 || program_.instructions.back().opcode != IrOpcode::end ||
                program_.instructions[program_.instructions.size() - 2].opcode != IrOpcode::store_output) {
            return fail("shader", "invalid_ir", "SPIR-V emission requires a terminal output store and end");
        }
        if (!is_constant(program_.exec, IrType::mask32, 0xffffffffu)) {
            return fail("shader", "unsupported_exec", "SPIR-V emission requires an all-active wave32 EXEC mask");
        }
        if (!is_constant(program_.vcc, IrType::mask32, 0) || !is_constant(program_.entry_scc, IrType::flag, 0)) {
            return fail("shader", "invalid_ir", "SPIR-V emission requires zero VCC and clear entry SCC");
        }

        declare_interface();
        for (index_ = 0; index_ < program_.instructions.size(); ++index_) {
            const auto& instruction = program_.instructions[index_];
            if (auto result = emit(instruction); !result) return std::unexpected(std::move(result.error()));
            // A single IR instruction allocates at most three fresh IDs.
            if (next_id_ > kMaxIdBound) return error("spirv_limit_exceeded", "SPIR-V result ID bound exceeded");
        }
        if (scc_ != program_.exit_scc) {
            return fail("shader", "invalid_ir", "SPIR-V emission found an inconsistent exit SCC value");
        }
        return finish();
    }

private:
    Word id() { return next_id_++; }

    template <class... Args>
    Word declare_type(Op opcode, Args... args) {
        const auto result = id();
        write(declarations_, opcode, {result, args...});
        return result;
    }

    template <class... Args>
    Word define(Words& section, Op opcode, Word type, Args... args) {
        const auto result = id();
        write(section, opcode, {type, result, args...});
        return result;
    }

    Word declare_buffer(Word array, Word binding, Word access) {
        const auto block = declare_type(Op::TypeStruct, array);
        const auto pointer = declare_type(Op::TypePointer, kStorageBuffer, block);
        const auto buffer = define(declarations_, Op::Variable, pointer, kStorageBuffer);
        write(annotations_, Op::Decorate, {block, kBlock});
        write(annotations_, Op::MemberDecorate, {block, 0, kOffset, 0});
        write(annotations_, Op::MemberDecorate, {block, 0, access});
        write(annotations_, Op::Decorate, {buffer, kDescriptorSet, 0});
        write(annotations_, Op::Decorate, {buffer, kBinding, binding});
        return buffer;
    }

    void declare_interface() {
        const auto void_type = declare_type(Op::TypeVoid);
        const auto function_type = declare_type(Op::TypeFunction, void_type);
        u32_ = declare_type(Op::TypeInt, Word{32}, Word{0});
        flag_ = declare_type(Op::TypeBool);
        zero_ = define(declarations_, Op::Constant, u32_, Word{0});
        word_bytes_ = define(declarations_, Op::Constant, u32_, kWordBytes);
        const auto count = define(declarations_, Op::Constant, u32_, kWaveSize);
        const auto array = declare_type(Op::TypeArray, u32_, count);
        const auto input_pointer = declare_type(Op::TypePointer, kInput, u32_);
        word_pointer_ = declare_type(Op::TypePointer, kStorageBuffer, u32_);
        lane_index_ = define(declarations_, Op::Variable, input_pointer, kInput);
        write(annotations_, Op::Decorate, {lane_index_, kBuiltIn, kLocalInvocationIndex});
        write(annotations_, Op::Decorate, {array, kArrayStride, kWordBytes});
        input_ = declare_buffer(array, 0, kNonWritable);
        output_ = declare_buffer(array, 1, kNonReadable);
        main_ = define(body_, Op::Function, void_type, Word{0}, function_type);
        write(body_, Op::Label, {id()});
    }

    Word float_type() {
        if (f32_ == 0) f32_ = declare_type(Op::TypeFloat, Word{32});
        return f32_;
    }

    SpirvModule finish() {
        Words module{kMagic, kVersion, 0, next_id_, 0};
        write(module, Op::Capability, {kShader});
        if (requires_rte_) write(module, Op::Capability, {kRteCapability});
        write(module, Op::MemoryModel, {kLogical, kGlsl450});
        // The literal string "main" is four bytes followed by a NUL word.
        write(module, Op::EntryPoint, {kCompute, main_, 0x6e69616d, 0, lane_index_, input_, output_});
        write(module, Op::ExecutionMode, {main_, kLocalSize, kWaveSize, 1, 1});
        if (requires_rte_) write(module, Op::ExecutionMode, {main_, kRteMode, 32});
        module.insert(module.end(), annotations_.begin(), annotations_.end());
        module.insert(module.end(), declarations_.begin(), declarations_.end());
        module.insert(module.end(), body_.begin(), body_.end());
        return {std::move(module), requires_rte_};
    }

    std::unexpected<Error> error(std::string_view code, std::string_view detail) const {
        const auto offset = program_.instructions[index_].dword_offset;
        if (offset) return fail("shader", code, std::format("SPIR-V emission at dword 0x{:x}: {}", *offset, detail));
        return fail("shader", code, std::format("SPIR-V emission at IR instruction {}: {}", index_, detail));
    }

    std::unexpected<Error> invalid(std::string_view detail) const { return error("invalid_ir", detail); }

    bool is_constant(IrValueId value, IrType type, Word bits) const {
        if (value >= program_.instructions.size()) return false;
        const auto& instruction = program_.instructions[value];
        return instruction.opcode == IrOpcode::constant && instruction.constant_bits == bits &&
               instruction.result == IrValueType{type, IrLanes::uniform};
    }

    bool pending(IrValueId value) const { return !last_wait_ || value > *last_wait_; }

    bool is_byte_offset(const IrInstruction& instruction) const {
        // Input definitions and arity have been checked before calling this.
        return instruction.opcode == IrOpcode::umul && !instruction.exec &&
               instruction.result == kLaneWord && instruction.input_count == 2 &&
               program_.instructions[instruction.inputs[0]].opcode == IrOpcode::lane_index &&
               is_constant(instruction.inputs[1], IrType::u32, kWordBytes);
    }

    bool shape(const IrInstruction& instruction, std::uint8_t arity,
               std::optional<IrValueType> result, bool predicated = false) const {
        return instruction.input_count == arity && instruction.result == result &&
               instruction.exec.has_value() == predicated;
    }

    IrValueType input_type(const IrInstruction& instruction, std::size_t operand) const {
        return *program_.instructions[instruction.inputs[operand]].result;
    }

    bool numeric_input(const IrInstruction& instruction, std::size_t operand, IrValueType result) const {
        const auto source = input_type(instruction, operand);
        return source.type == result.type && (source.lanes == result.lanes || source.lanes == IrLanes::uniform);
    }

    Result<void> emit_numeric(const IrInstruction& instruction, Op opcode, IrType type, std::uint8_t arity) {
        if (!instruction.result || instruction.input_count != arity || instruction.result->type != type) {
            return invalid("incorrect numeric instruction result or arity");
        }
        const auto result = *instruction.result;
        if (result.lanes != IrLanes::uniform && result.lanes != IrLanes::per_lane) {
            return invalid("unknown IR lane mode");
        }
        const bool predicated = result.lanes == IrLanes::per_lane && !is_byte_offset(instruction);
        if (instruction.exec.has_value() != predicated || (type == IrType::f32 && !predicated)) {
            return error("unsupported_exec", "numeric instruction has an invalid EXEC operand");
        }
        for (std::uint8_t i = 0; i < arity; ++i) {
            if (!numeric_input(instruction, i, result)) return invalid("incorrect numeric operand type or lane mode");
        }
        const auto first = ids_[instruction.inputs[0]];
        if (arity == 1) {
            ids_[index_] = define(body_, opcode, u32_, first);
        } else {
            const auto type_id = type == IrType::f32 ? float_type() : u32_;
            ids_[index_] = define(body_, opcode, type_id, first, ids_[instruction.inputs[1]]);
        }
        if (type == IrType::f32) {
            requires_rte_ = true;
            write(annotations_, Op::Decorate, {ids_[index_], kNoContraction});
        }
        return {};
    }

    Result<void> emit_constant(const IrInstruction& instruction) {
        if (!instruction.result || instruction.result->lanes != IrLanes::uniform ||
                instruction.input_count != 0 || instruction.exec) {
            return invalid("constant must have a uniform result and no operands");
        }
        switch (instruction.result->type) {
        case IrType::flag:
            if (instruction.constant_bits > 1) return invalid("flag constant must be zero or one");
            ids_[index_] = define(declarations_, instruction.constant_bits ? Op::ConstantTrue : Op::ConstantFalse, flag_);
            return {};
        case IrType::u32:
        case IrType::mask32:
            ids_[index_] = define(declarations_, Op::Constant, u32_, instruction.constant_bits);
            return {};
        case IrType::f32: break;
        }
        return invalid("ABI v0 constants use raw bits, flags or masks");
    }

    Result<void> emit_bitcast(const IrInstruction& instruction) {
        if (!instruction.result || instruction.input_count != 1 || instruction.exec) {
            return invalid("bitcast requires one input and no EXEC operand");
        }
        const auto source = input_type(instruction, 0);
        const auto type = instruction.result->type;
        const bool to_float = type == IrType::f32 && source.type == IrType::u32;
        const bool to_word = type == IrType::u32 && source.type == IrType::f32;
        if ((!to_float && !to_word) || instruction.result->lanes != source.lanes) {
            return invalid("bitcast must reinterpret u32 and f32 with the same lane mode");
        }
        ids_[index_] = define(body_, Op::Bitcast, to_float ? float_type() : u32_, ids_[instruction.inputs[0]]);
        return {};
    }

    Result<void> emit_buffer(const IrInstruction& instruction, bool load) {
        const auto result = load ? std::optional{kLaneWord} : std::nullopt;
        if (!shape(instruction, load ? 1 : 2, result, true) ||
                !is_byte_offset(program_.instructions[instruction.inputs[0]])) {
            return invalid("buffer access requires EXEC and the ABI lane byte offset");
        }
        if (!load && (input_type(instruction, 1) != kLaneWord || index_ != program_.instructions.size() - 2 ||
                      (last_load_ && pending(*last_load_)))) {
            return invalid("output store requires per-lane dwords after all loads have been waited on");
        }
        const auto element = define(body_, Op::UDiv, u32_, ids_[instruction.inputs[0]], word_bytes_);
        const auto address = define(body_, Op::AccessChain, word_pointer_, load ? input_ : output_, zero_, element);
        if (load) {
            ids_[index_] = define(body_, Op::Load, u32_, address);
            last_load_ = index_;
        } else {
            write(body_, Op::Store, {address, ids_[instruction.inputs[1]]});
        }
        return {};
    }

    Result<void> emit(const IrInstruction& instruction) {
        if (instruction.input_count > instruction.inputs.size()) return invalid("input count exceeds IR operand storage");
        if (instruction.exec && (*instruction.exec != program_.exec || program_.exec >= index_)) {
            return error("unsupported_exec", "expected a reference to the declared all-active EXEC mask");
        }
        for (const auto input : std::span(instruction.inputs).first(instruction.input_count)) {
            if (input >= index_ || ids_[input] == 0) return invalid("input must refer to an earlier value definition");
            if (program_.instructions[input].opcode == IrOpcode::load_input && pending(input)) {
                return invalid("input load is used before wait_vmem");
            }
        }

        switch (instruction.opcode) {
        case IrOpcode::constant: return emit_constant(instruction);
        case IrOpcode::lane_index:
            if (!shape(instruction, 0, kLaneWord)) return invalid("lane index must be a per-lane u32");
            ids_[index_] = define(body_, Op::Load, u32_, lane_index_);
            return {};
        case IrOpcode::copy: return emit_numeric(instruction, Op::CopyObject, IrType::u32, 1);
        case IrOpcode::bitcast: return emit_bitcast(instruction);
        case IrOpcode::uadd: return emit_numeric(instruction, Op::IAdd, IrType::u32, 2);
        case IrOpcode::umul: return emit_numeric(instruction, Op::IMul, IrType::u32, 2);
        case IrOpcode::fadd: return emit_numeric(instruction, Op::FAdd, IrType::f32, 2);
        case IrOpcode::fmul: return emit_numeric(instruction, Op::FMul, IrType::f32, 2);
        case IrOpcode::uadd_carry: {
            constexpr IrValueType uniform_word{IrType::u32, IrLanes::uniform};
            if (!shape(instruction, 2, kUniformFlag) || input_type(instruction, 0) != uniform_word ||
                    input_type(instruction, 1) != uniform_word) {
                return invalid("SCC carry requires two uniform u32 inputs and a uniform flag result");
            }
            ids_[index_] = define(body_, Op::ULessThan, flag_, ids_[instruction.inputs[0]], ids_[instruction.inputs[1]]);
            scc_ = index_;
            return {};
        }
        case IrOpcode::load_input: return emit_buffer(instruction, true);
        case IrOpcode::wait_vmem:
            if (!shape(instruction, 0, std::nullopt)) return invalid("wait_vmem cannot have operands or a result");
            // SPIR-V loads are synchronous SSA definitions. The validated wait
            // adds no memory barrier between work-items under this ABI.
            last_wait_ = index_;
            return {};
        case IrOpcode::store_output: return emit_buffer(instruction, false);
        case IrOpcode::end:
            if (!shape(instruction, 0, std::nullopt) || index_ != program_.instructions.size() - 1) {
                return invalid("end must be the final instruction and have no operands or result");
            }
            write(body_, Op::Return, {});
            write(body_, Op::FunctionEnd, {});
            return {};
        }
        return error("unsupported_ir_opcode", "unknown IR opcode");
    }

    const IrProgram& program_;
    Words ids_;
    IrValueId index_{};
    IrValueId scc_;
    std::optional<IrValueId> last_wait_;
    std::optional<IrValueId> last_load_;
    Words annotations_;
    Words declarations_;
    Words body_;
    Word next_id_ = 1;
    Word u32_{};
    Word flag_{};
    Word f32_{};
    Word zero_{};
    Word word_bytes_{};
    Word word_pointer_{};
    Word lane_index_{};
    Word input_{};
    Word output_{};
    Word main_{};
    bool requires_rte_{};
};

}  // namespace

Result<SpirvModule> emit_spirv(const IrProgram& program) {
    return Emitter(program).run();
}

}  // namespace scale::shader
