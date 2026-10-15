#include "MathState.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRadiusRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ItemStats.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/ObjectClient/VisibleItem_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/PaperDollInfoFrame.h"
#include "WoW/Source/Ui/PetInfo.h"
#include "WoW/Source/Ui/QuestTextParser.h"
#include "WoW/Source/Duskhaven/Character/CharacterDefines.h"
#include "WoW/Source/Duskhaven/DBC/Defs/SpellEffect.h"
#include "WoW/Source/Duskhaven/DBC/Defs/SpellEffectScalars.h"
#include "WoW/Source/Duskhaven/Tooltip/SpellTooltipExtensions.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

// STATIC: 0x00ACE898
static const MathFunction s_mathFunctions[12] = {
    { "abs", 1 },
    { "ceil", 1 },
    { "floor", 1 },
    { "min", 2 },
    { "max", 2 },
    { "gt", 2 },
    { "lt", 2 },
    { "gte", 2 },
    { "lte", 2 },
    { "eq", 2 },
    { "cond", 3 },
    { "clamp", 3 },
};

// STATIC: 0x00ACE8F8
static const char* s_mathVariables[140] = {
    "STR", "AGI", "STA", "INT", "SPI", "str", "agi", "sta", "int", "spi",
    "m1", "m2", "m3", "M1", "M2", "M3", "a1", "a2", "a3", "A1", "A2", "A3",
    "d", "D", "c", "C", "p", "P", "x1", "x2", "x3", "X1", "X2", "X3",
    "t1", "t2", "t3", "T1", "T2", "T3", "h", "H", "n", "N",
    "b1", "b2", "b3", "B1", "B2", "B3", "u", "U", "v", "V",
    "e1", "e2", "e3", "E1", "E2", "E3", "i", "I", "f1", "f2", "f3", "F1", "F2", "F3",
    "q1", "q2", "q3", "Q1", "Q2", "Q3", "ap", "AP", "rap", "RAP",
    "mwb", "MWB", "owb", "OWB", "rwb", "RWB", "mw", "MW", "ow", "OW", "rw", "RW",
    "ar", "AR", "mws", "MWS", "ows", "OWS", "rws", "RWS", "pl", "PL", "hnd", "HND",
    "sp", "SP", "sph", "spfi", "spn", "spfr", "sps", "spa", "SPH", "SPFI", "SPN", "SPFR", "SPS", "SPA",
    "bh", "BH", "ph", "pfi", "pn", "pfr", "ps", "pa", "PH", "PFI", "PN", "PFR", "PS", "PA",
    "pbh", "pBH", "pbhd", "pBHD", "bc1", "bc2", "bc3", "BC1", "BC2", "BC3",
};

// STATIC: 0x00ACEB2C
static const char* s_mathDelimiters = "()[]^*/%+-#,> ";

// VERIFY: 419474d0 auto
// OFFSET: 0x00576B10
static uint8_t LookupMathFunction(const char* name, int32_t* argCount) {
    for (uint32_t i = 0; i < 12; i++) {
        if (!SStrCmpI(name, s_mathFunctions[i].m_name, STORM_MAX_STR)) {
            *argCount = s_mathFunctions[i].m_argCount;
            return i + MATHOP_ABS;
        }
    }

    *argCount = 0;
    return 0;
}

// VERIFY: 924c0932 auto
// OFFSET: 0x00576B50
static uint16_t LookupMathVariable(const char* name) {
    if (!name || !*name) {
        return 0;
    }

    for (uint32_t i = 0; i < 140; i++) {
        if (!SStrCmp(name, s_mathVariables[i], STORM_MAX_STR)) {
            return static_cast<uint16_t>(i + MATHOP_STAT_STRENGTH);
        }
    }

    if (!SStrCmp(name, "hp", STORM_MAX_STR)) return MATHOP_EXT_HP;
    if (!SStrCmp(name, "HP", STORM_MAX_STR)) return MATHOP_EXT_HP_UPPER;
    if (!SStrCmp(name, "dpct", STORM_MAX_STR)) return MATHOP_EXT_DPCT;
    if (!SStrCmp(name, "ppct", STORM_MAX_STR)) return MATHOP_EXT_PPCT;
    if (!SStrCmp(name, "MASTERY", STORM_MAX_STR)) return MATHOP_EXT_MASTERY;

    if (!std::strncmp(name, "mastery", 7) && std::isdigit(static_cast<unsigned char>(name[7])) && name[8] == '\0') {
        int idx = name[7] - '0';
        if (idx >= 1 && idx <= 4) return static_cast<uint16_t>(MATHOP_EXT_MASTERY_1 + (idx - 1));
    }

    if (!std::strncmp(name, "power", 5) && std::isdigit(static_cast<unsigned char>(name[5])) && name[6] == '\0') {
        int idx = name[5] - '0';
        if (idx >= 1 && idx <= 7) return static_cast<uint16_t>(MATHOP_EXT_POWER_1 + (idx - 1));
    }

    if (!std::strncmp(name, "POWER", 5) && std::isdigit(static_cast<unsigned char>(name[5])) && name[6] == '\0') {
        int idx = name[5] - '0';
        if (idx >= 1 && idx <= 7) return static_cast<uint16_t>(MATHOP_EXT_POWER_1_UPPER + (idx - 1));
    }

    auto parseIndex = [](const char* str, size_t prefixLen, uint32_t& outIdx) -> bool {
        const char* p = str + prefixLen;
        if (!*p || !std::isdigit(static_cast<unsigned char>(*p))) return false;
        char* end = nullptr;
        unsigned long val = std::strtoul(p, &end, 10);
        if (!end || *end != '\0' || val < 1 || val > 32) return false;
        outIdx = static_cast<uint32_t>(val);
        return true;
    };

    uint32_t idx = 0;
    if (!std::strncmp(name, "ppl", 3) && parseIndex(name, 3, idx)) return static_cast<uint16_t>(MATHOP_EXT_PPL_1 + (idx - 1));
    if (!std::strncmp(name, "PPL", 3) && parseIndex(name, 3, idx)) return static_cast<uint16_t>(MATHOP_EXT_PPL_1_UPPER + (idx - 1));
    if (!std::strncmp(name, "bon", 3) && parseIndex(name, 3, idx)) return static_cast<uint16_t>(MATHOP_EXT_BON_1 + (idx - 1));

    if (!std::strncmp(name, "bc", 2) && parseIndex(name, 2, idx)) return static_cast<uint16_t>(MATHOP_EXT_BC_1 + (idx - 1));
    if (!std::strncmp(name, "BC", 2) && parseIndex(name, 2, idx)) return static_cast<uint16_t>(MATHOP_EXT_BC_1_UPPER + (idx - 1));

    if (name[0] == 'm' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_M_1 + (idx - 1));
    if (name[0] == 'M' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_M_1_UPPER + (idx - 1));
    if (name[0] == 'a' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_A_1 + (idx - 1));
    if (name[0] == 'A' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_A_1_UPPER + (idx - 1));
    if (name[0] == 'x' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_X_1 + (idx - 1));
    if (name[0] == 'X' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_X_1_UPPER + (idx - 1));
    if (name[0] == 't' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_T_1 + (idx - 1));
    if (name[0] == 'T' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_T_1_UPPER + (idx - 1));
    if (name[0] == 'b' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_B_1 + (idx - 1));
    if (name[0] == 'B' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_B_1_UPPER + (idx - 1));
    if (name[0] == 'e' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_E_1 + (idx - 1));
    if (name[0] == 'E' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_E_1_UPPER + (idx - 1));
    if (name[0] == 'f' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_F_1 + (idx - 1));
    if (name[0] == 'F' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_F_1_UPPER + (idx - 1));
    if (name[0] == 'q' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_Q_1 + (idx - 1));
    if (name[0] == 'Q' && parseIndex(name, 1, idx)) return static_cast<uint16_t>(MATHOP_EXT_Q_1_UPPER + (idx - 1));

    return 0;
}

// VERIFY: 217b88e2 auto
// OFFSET: 0x00576B90
uint32_t MathState::AddIntArg(int32_t value) {
    uint32_t index = this->m_numIntArgs;
    this->m_intArgs[index] = value;
    this->m_numIntArgs++;
    return index;
}

// VERIFY: 307e2467 auto
// OFFSET: 0x00576BB0
int32_t MathState::NextToken(char* token, int32_t* pos, int32_t* length) {
    *length = 0;
    const char* text = this->m_text + *pos;

    while (*text == ' ') {
        (*length)++;
        text++;
    }

    if (!*text) {
        *pos += *length;
        return 0;
    }

    const char* delimiter = SStrChr(s_mathDelimiters, *text);

    if (delimiter) {
        token[0] = *delimiter;
        token[1] = '\0';
        (*length)++;
        *pos += *length;
        return 1;
    }

    char* out = token;

    while (*text && !SStrChr(s_mathDelimiters, *text)) {
        *out++ = *text++;
        (*length)++;
    }

    *out = '\0';
    *pos += *length;
    return 1;
}

// VERIFY: 364d741d auto
// OFFSET: 0x00576C80
int32_t MathState::Expect(const char* expected, int32_t* pos) {
    char token[32];
    int32_t length;

    if (!this->NextToken(token, pos, &length) || SStrCmpI(token, expected, STORM_MAX_STR)) {
        this->m_status = 1;
        return 0;
    }

    return 1;
}

// VERIFY: d1d41ba2 auto
// OFFSET: 0x005774D0
static float GetSpellVariableValue(const char* name) {
    if (!name || !*name) {
        return 0.0f;
    }

    for (uint32_t i = 0; i < s_spellVariables.m_count; i++) {
        if (!SStrCmp(s_spellVariables.m_data[i].m_name, name, STORM_MAX_STR)) {
            return s_spellVariables.m_data[i].m_value;
        }
    }

    return 0.0f;
}

// VERIFY: 53ef7008 manual
// OFFSET: 0x00577530
int32_t MathState::CompilePrimary(int32_t pos) {
    char token[32];
    char separator[32];
    char digits[8];
    int32_t length;

    if (!this->NextToken(token, &pos, &length)) {
        return pos;
    }

    char first = token[0];

    if (!token[1]) {
        if (first == '(') {
            pos = this->CompileSum(pos);
            this->Expect(")", &pos);
            return pos;
        }

        if (first == '-') {
            int32_t result = this->CompilePrimary(pos);
            this->m_ops[this->m_numOps] = MATHOP_NEGATE;
            this->m_numOps++;
            return result;
        }
    }

    if (first == '$') {
        const char* name = &token[1];

        if (token[1] == '<') {
            this->m_variables[this->m_numVariables] = GetSpellVariableValue(&token[2]);
            this->m_numVariables++;
            this->m_ops[this->m_numOps] = MATHOP_SPELL_VARIABLE;
            this->m_numOps++;
            this->Expect(">", &pos);
            return pos;
        }

        int32_t argCount;
        uint8_t function = LookupMathFunction(name, &argCount);

        if (function) {
            this->Expect("(", &pos);
            pos = this->CompileSum(pos);

            for (int32_t i = argCount - 1; i; i--) {
                if (!this->NextToken(separator, &pos, &length) || SStrCmpI(separator, ",", STORM_MAX_STR)) {
                    this->m_status = 1;
                }

                pos = this->CompileSum(pos);
            }

            this->m_ops[this->m_numOps] = function;
            this->m_numOps++;
            this->Expect(")", &pos);
            return pos;
        }

        if (isdigit(token[1]) && token[1]) {
            int32_t count = 0;

            while (token[1 + count] >= '0' && token[1 + count] <= '9') {
                digits[count] = token[1 + count];
                count++;
            }

            if (count) {
                digits[count] = '\0';
                this->AddIntArg(SStrToInt(digits));
                this->m_ops[this->m_numOps] = MATHOP_SPELL;
                this->m_numOps++;
                name = &token[1 + count];
            }
        }

        uint16_t variable = LookupMathVariable(name);

        if (variable) {
            this->m_ops[this->m_numOps] = variable;
            this->m_numOps++;
            return pos;
        }
    }

    if (!isdigit(first) && first != '.') {
        this->m_status = 1;
        return pos;
    }

    this->m_constants[this->m_numConstants] = static_cast<float>(atof(token));
    this->m_numConstants++;
    this->m_ops[this->m_numOps] = MATHOP_CONSTANT;
    this->m_numOps++;
    return pos;
}

// VERIFY: c2929519 auto
// OFFSET: 0x005777A0
int32_t MathState::CompilePower(int32_t pos) {
    pos = this->CompilePrimary(pos);

    while (this->m_text[pos] == '^') {
        pos = this->CompilePrimary(pos + 1);
        this->m_ops[this->m_numOps] = MATHOP_POW;
        this->m_numOps++;
    }

    return pos;
}

// VERIFY: b6d928a9 manual
// OFFSET: 0x005777F0
int32_t MathState::CompileProduct(int32_t pos) {
    pos = this->CompilePower(pos);

    while (true) {
        char op = this->m_text[pos];

        if (op != '*' && op != '/' && op != '%') {
            break;
        }

        pos = this->CompilePrimary(pos + 1);

        while (this->m_text[pos] == '^') {
            pos = this->CompilePrimary(pos + 1);
            this->m_ops[this->m_numOps] = MATHOP_POW;
            this->m_numOps++;
        }

        if (op == '%') {
            this->m_ops[this->m_numOps] = MATHOP_MOD;
        } else if (op == '*') {
            this->m_ops[this->m_numOps] = MATHOP_MUL;
        } else {
            this->m_ops[this->m_numOps] = MATHOP_DIV;
        }

        this->m_numOps++;
    }

    return pos;
}

// VERIFY: 99fe2678 auto
// OFFSET: 0x005778B0
int32_t MathState::CompileSum(int32_t pos) {
    pos = this->CompileProduct(pos);

    while (true) {
        char op = this->m_text[pos];

        if (op != '+' && op != '-') {
            break;
        }

        pos = this->CompileProduct(pos + 1);
        this->m_ops[this->m_numOps] = (op != '+') + MATHOP_ADD;
        this->m_numOps++;
    }

    return pos;
}

// VERIFY: fa170935 manual
// OFFSET: 0x00577900
void MathState::ExecuteFunction(uint32_t op, MathStack* stack) {
    float args[3];

    for (int32_t i = s_mathFunctions[op - MATHOP_ABS].m_argCount; i; stack->m_top++) {
        i--;
        args[i] = stack->m_values[stack->m_top];
    }

    float result;

    switch (op) {
        case MATHOP_ABS:
            result = fabsf(args[0]);
            break;

        case MATHOP_CEIL:
            result = ceilf(args[0]);
            break;

        case MATHOP_FLOOR:
            result = static_cast<float>(floor(args[0]));
            break;

        case MATHOP_MIN:
            result = args[0] < args[1] ? args[0] : args[1];
            break;

        case MATHOP_MAX:
            result = args[0] > args[1] ? args[0] : args[1];
            break;

        case MATHOP_GT:
            result = args[1] >= args[0] ? 0.0f : 1.0f;
            break;

        case MATHOP_LT:
            result = args[1] <= args[0] ? 0.0f : 1.0f;
            break;

        case MATHOP_GTE:
            result = args[1] > args[0] ? 0.0f : 1.0f;
            break;

        case MATHOP_LTE:
            result = args[1] < args[0] ? 0.0f : 1.0f;
            break;

        case MATHOP_EQ:
            result = fabsf(args[0] - args[1]) >= 0.0001f ? 0.0f : 1.0f;
            break;

        case MATHOP_COND:
            result = args[0] == 0.0f ? args[2] : args[1];
            break;

        case MATHOP_CLAMP: {
            float upper = args[1] <= args[0] ? args[0] : args[1];
            result = args[0];

            if (args[2] <= upper) {
                result = args[2];
            } else if (args[1] > result) {
                result = args[1];
            }

            break;
        }

        default:
            this->m_status = 1;
            result = 0.0f;
            break;
    }

    stack->m_top--;
    stack->m_values[stack->m_top] = result;
}

// VERIFY: e745ead3 auto
// OFFSET: 0x00577AD0
void MathState::ExecuteOperator(uint32_t op, MathStack* stack) {
    float result;

    switch (op) {
        case MATHOP_POW: {
            float exponent = stack->m_values[stack->m_top++];
            float base = stack->m_values[stack->m_top++];
            result = static_cast<float>(pow(base, exponent));
            break;
        }

        case MATHOP_NEGATE:
            result = -stack->m_values[stack->m_top++];
            break;

        case MATHOP_MUL: {
            float right = stack->m_values[stack->m_top++];
            float left = stack->m_values[stack->m_top++];
            result = right * left;
            break;
        }

        case MATHOP_DIV: {
            float right = stack->m_values[stack->m_top++];
            float left = stack->m_values[stack->m_top++];
            result = left / right;
            break;
        }

        case MATHOP_MOD: {
            float right = stack->m_values[stack->m_top++];
            float left = stack->m_values[stack->m_top++];
            double quotient = left / right;
            float fraction = static_cast<float>(quotient);
            result = static_cast<float>(left - (fraction - floor(quotient)) * right);
            break;
        }

        case MATHOP_ADD: {
            float right = stack->m_values[stack->m_top++];
            float left = stack->m_values[stack->m_top++];
            result = right + left;
            break;
        }

        case MATHOP_SUB: {
            float right = stack->m_values[stack->m_top++];
            float left = stack->m_values[stack->m_top++];
            result = left - right;
            break;
        }

        default:
            result = 0.0f;
            this->m_status = 1;
            break;
    }

    stack->m_top--;
    stack->m_values[stack->m_top] = result;
}

// VERIFY: ef1f3713 manual
void MathState::GetVariableValue(uint32_t op, MathStack* stack, const SpellRec* spell, int32_t spellLevel, int32_t scale, int32_t parseUnit, int32_t parsePet, int32_t noModifiers, bool isProjected) {
    float value = 0.0f;
    CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(ClntObjMgrGetActivePlayer(), TYPEMASK_PLAYER, __FILE__, __LINE__));

    // Check if a stock effect variable references an effect cached in SpellEffectCacheDB
    if (!isProjected && spell) {
        uint32_t effectIdx = 0;
        uint16_t nativeSlotOne = 0;
        bool isEffectOp = false;

        struct StockEffectRange { uint16_t first; uint16_t count; uint16_t nativeSlotOne; };
        static const StockEffectRange stockRanges[] = {
            { MATHOP_MIN_POINTS_1, 3, MATHOP_MIN_POINTS_1 },
            { MATHOP_MAX_POINTS_1, 3, MATHOP_MAX_POINTS_1 },
            { MATHOP_RADIUS_1, 3, MATHOP_RADIUS_1 },
            { MATHOP_RADIUS_1_UPPER, 3, MATHOP_RADIUS_1_UPPER },
            { MATHOP_CHAIN_TARGETS_1, 3, MATHOP_CHAIN_TARGETS_1 },
            { MATHOP_CHAIN_TARGETS_1_UPPER, 3, MATHOP_CHAIN_TARGETS_1_UPPER },
            { MATHOP_AURA_PERIOD_1, 3, MATHOP_AURA_PERIOD_1 },
            { MATHOP_AURA_PERIOD_1_UPPER, 3, MATHOP_AURA_PERIOD_1_UPPER },
            { MATHOP_POINTS_PER_COMBO_1, 3, MATHOP_POINTS_PER_COMBO_1 },
            { MATHOP_POINTS_PER_COMBO_1_UPPER, 3, MATHOP_POINTS_PER_COMBO_1_UPPER },
            { MATHOP_AMPLITUDE_1, 3, MATHOP_AMPLITUDE_1 },
            { MATHOP_AMPLITUDE_1_UPPER, 3, MATHOP_AMPLITUDE_1_UPPER },
            { MATHOP_CHAIN_AMPLITUDE_1, 3, MATHOP_CHAIN_AMPLITUDE_1 },
            { MATHOP_CHAIN_AMPLITUDE_1_UPPER, 3, MATHOP_CHAIN_AMPLITUDE_1_UPPER },
            { MATHOP_MISC_VALUE_1, 3, MATHOP_MISC_VALUE_1 },
            { MATHOP_MISC_VALUE_1_UPPER, 3, MATHOP_MISC_VALUE_1_UPPER },
            { MATHOP_BONUS_COEFFICIENT_1, 3, MATHOP_BONUS_COEFFICIENT_1 },
            { MATHOP_BONUS_COEFFICIENT_1_UPPER, 3, MATHOP_BONUS_COEFFICIENT_1_UPPER }
        };

        for (const auto& r : stockRanges) {
            if (op >= r.first && op < r.first + r.count) {
                effectIdx = op - r.first;
                nativeSlotOne = r.nativeSlotOne;
                isEffectOp = true;
                break;
            }
        }

        if (isEffectOp) {
            SpellEffectCacheRow* customEffect = SpellEffectCacheDB::Get(SpellEffectCacheKey(spell->m_id, effectIdx));
            if (customEffect) {
                SpellRec projected;
                Duskhaven::Tooltip::ProjectEffectIntoFirstSlot(spell, *customEffect, projected);
                this->GetVariableValue(nativeSlotOne, stack, &projected, spellLevel, scale, parseUnit, parsePet, noModifiers, true);
                return;
            }
        }
    }

    // Duskhaven dynamic effect variables (indices 1..32 for m, M, a, A, x, X, t, T, b, B, e, E, f, F, q, Q, bc, BC)
    if (op >= MATHOP_EXT_M_1 && op <= MATHOP_EXT_BC_32_UPPER) {
        uint32_t offset = op - MATHOP_EXT_M_1;
        uint32_t group = offset / 32;
        uint32_t effectIdx = offset % 32;

        static const uint16_t nativeSlotOneVariables[18] = {
            MATHOP_MIN_POINTS_1,
            MATHOP_MAX_POINTS_1,
            MATHOP_RADIUS_1,
            MATHOP_RADIUS_1_UPPER,
            MATHOP_CHAIN_TARGETS_1,
            MATHOP_CHAIN_TARGETS_1_UPPER,
            MATHOP_AURA_PERIOD_1,
            MATHOP_AURA_PERIOD_1_UPPER,
            MATHOP_POINTS_PER_COMBO_1,
            MATHOP_POINTS_PER_COMBO_1_UPPER,
            MATHOP_AMPLITUDE_1,
            MATHOP_AMPLITUDE_1_UPPER,
            MATHOP_CHAIN_AMPLITUDE_1,
            MATHOP_CHAIN_AMPLITUDE_1_UPPER,
            MATHOP_MISC_VALUE_1,
            MATHOP_MISC_VALUE_1_UPPER,
            MATHOP_BONUS_COEFFICIENT_1,
            MATHOP_BONUS_COEFFICIENT_1_UPPER
        };

        uint16_t nativeSlotOne = nativeSlotOneVariables[group];

        SpellEffectCacheRow* customEffect = (spell && !isProjected) ? SpellEffectCacheDB::Get(SpellEffectCacheKey(spell->m_id, effectIdx)) : nullptr;
        if (customEffect) {
            SpellRec projected;
            Duskhaven::Tooltip::ProjectEffectIntoFirstSlot(spell, *customEffect, projected);
            this->GetVariableValue(nativeSlotOne, stack, &projected, spellLevel, scale, parseUnit, parsePet, noModifiers, true);
            return;
        } else if (effectIdx < 3) {
            this->GetVariableValue(nativeSlotOne + effectIdx, stack, spell, spellLevel, scale, parseUnit, parsePet, noModifiers, isProjected);
            return;
        } else {
            stack->m_top--;
            stack->m_values[stack->m_top] = 0.0f;
            return;
        }
    }

    if (player) {
        switch (op) {
            case MATHOP_STAT_STRENGTH:
            case MATHOP_STAT_AGILITY:
            case MATHOP_STAT_STAMINA:
            case MATHOP_STAT_INTELLECT:
            case MATHOP_STAT_SPIRIT:
                value = static_cast<float>(player->GetEffectiveStat(op - MATHOP_STAT_STRENGTH));
                break;

            case MATHOP_BASE_STAT_STRENGTH:
            case MATHOP_BASE_STAT_AGILITY:
            case MATHOP_BASE_STAT_STAMINA:
            case MATHOP_BASE_STAT_INTELLECT:
            case MATHOP_BASE_STAT_SPIRIT:
                value = static_cast<float>(player->m_unit->m_unitFieldStat[op - MATHOP_BASE_STAT_STRENGTH] - player->m_unit->m_unitFieldPosstat[op - MATHOP_BASE_STAT_STRENGTH] - player->m_unit->m_unitFieldNegstat[op - MATHOP_BASE_STAT_STRENGTH]);
                break;

            case MATHOP_MIN_POINTS_1:
            case MATHOP_MIN_POINTS_2:
            case MATHOP_MIN_POINTS_3: {
                float minPoints;
                float maxPoints;
                Spell_C_GetMinMaxPoints(spell, op - MATHOP_MIN_POINTS_1, &minPoints, &maxPoints, spellLevel, parsePet, parseUnit, noModifiers);
                value = scale * minPoints;
                break;
            }

            case MATHOP_MAX_POINTS_1:
            case MATHOP_MAX_POINTS_2:
            case MATHOP_MAX_POINTS_3: {
                float minPoints;
                float maxPoints;
                Spell_C_GetMinMaxPoints(spell, op - MATHOP_MAX_POINTS_1, &minPoints, &maxPoints, spellLevel, parsePet, parseUnit, noModifiers);
                value = scale * maxPoints;
                break;
            }

            case MATHOP_RADIUS_1:
            case MATHOP_RADIUS_2:
            case MATHOP_RADIUS_3:
            case MATHOP_RADIUS_1_UPPER:
            case MATHOP_RADIUS_2_UPPER:
            case MATHOP_RADIUS_3_UPPER: {
                float radius = 0.0f;
                uint32_t index = op < MATHOP_RADIUS_1_UPPER ? op - MATHOP_RADIUS_1 : op - MATHOP_RADIUS_1_UPPER;

                if (index < 3) {
                    SpellRadiusRec* rec = g_spellRadiusDB.GetRecord(spell->m_effectRadiusIndex[index]);

                    if (rec) {
                        radius = rec->m_radius;
                        Spell_C_ModifySpellValue(spell, radius, 6);
                    }
                }

                value = radius;
                break;
            }

            case MATHOP_DURATION:
            case MATHOP_DURATION_UPPER:
                value = Spell_C_GetDuration(spell, parsePet, parseUnit, noModifiers, 1) * 0.001f;
                break;

            case MATHOP_POWER_COST:
            case MATHOP_POWER_COST_UPPER: {
                CGUnit_C* unit;

                if (parseUnit) {
                    unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(CGCharacterInfo::m_inspectTarget, TYPEMASK_UNIT, __FILE__, __LINE__));
                } else if (parsePet) {
                    unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(CGPetInfo::GetPet(0), TYPEMASK_UNIT, __FILE__, __LINE__));
                } else {
                    unit = player;
                }

                value = static_cast<float>(Spell_C_GetPowerCost(spell, unit));
                break;
            }

            case MATHOP_POWER_COST_PER_SECOND:
            case MATHOP_POWER_COST_PER_SECOND_UPPER: {
                CGUnit_C* unit;

                if (parseUnit) {
                    unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(CGCharacterInfo::m_inspectTarget, TYPEMASK_UNIT, __FILE__, __LINE__));
                } else if (parsePet) {
                    unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(CGPetInfo::GetPet(0), TYPEMASK_UNIT, __FILE__, __LINE__));
                } else {
                    unit = player;
                }

                value = static_cast<float>(Spell_C_GetPowerCostPerSecond(spell, unit));
                break;
            }

            case MATHOP_CHAIN_TARGETS_1:
            case MATHOP_CHAIN_TARGETS_2:
            case MATHOP_CHAIN_TARGETS_3:
            case MATHOP_CHAIN_TARGETS_1_UPPER:
            case MATHOP_CHAIN_TARGETS_2_UPPER:
            case MATHOP_CHAIN_TARGETS_3_UPPER: {
                int32_t targets = 0;
                uint32_t index = op < MATHOP_CHAIN_TARGETS_1_UPPER ? op - MATHOP_CHAIN_TARGETS_1 : op - MATHOP_CHAIN_TARGETS_1_UPPER;

                if (index < 3) {
                    targets = spell->m_effectChainTargets[index];
                    Spell_C_ModifySpellValue(spell, targets, 17);
                }

                value = static_cast<float>(targets);
                break;
            }

            case MATHOP_AURA_PERIOD_1:
            case MATHOP_AURA_PERIOD_2:
            case MATHOP_AURA_PERIOD_3:
            case MATHOP_AURA_PERIOD_1_UPPER:
            case MATHOP_AURA_PERIOD_2_UPPER:
            case MATHOP_AURA_PERIOD_3_UPPER: {
                int32_t period = 0;

                if (spell->m_procTypeMask & 1) {
                    period = 5000;
                } else {
                    uint32_t index = op < MATHOP_AURA_PERIOD_1_UPPER ? op - MATHOP_AURA_PERIOD_1 : op - MATHOP_AURA_PERIOD_1_UPPER;

                    if (index < 3) {
                        period = spell->m_effectAuraPeriod[index];
                        Spell_C_ModifySpellValue(spell, period, 19);
                    }
                }

                value = static_cast<float>(period);
                break;
            }

            case MATHOP_PROC_CHANCE:
            case MATHOP_PROC_CHANCE_UPPER: {
                int32_t chance = spell->m_procChance;
                Spell_C_ModifySpellValue(spell, chance, 18);
                value = static_cast<float>(chance);
                break;
            }

            case MATHOP_PROC_CHARGES:
            case MATHOP_PROC_CHARGES_UPPER: {
                int32_t charges = spell->m_procCharges;
                Spell_C_ModifySpellValue(spell, charges, 4);
                value = static_cast<float>(charges);
                break;
            }

            case MATHOP_POINTS_PER_COMBO_1:
            case MATHOP_POINTS_PER_COMBO_2:
            case MATHOP_POINTS_PER_COMBO_3:
            case MATHOP_POINTS_PER_COMBO_1_UPPER:
            case MATHOP_POINTS_PER_COMBO_2_UPPER:
            case MATHOP_POINTS_PER_COMBO_3_UPPER: {
                uint32_t index = op < MATHOP_POINTS_PER_COMBO_1_UPPER ? op - MATHOP_POINTS_PER_COMBO_1 : op - MATHOP_POINTS_PER_COMBO_1_UPPER;

                if (index < 3) {
                    value = spell->m_effectPointsPerCombo[index];
                }

                break;
            }

            case MATHOP_CUMULATIVE_AURA:
            case MATHOP_CUMULATIVE_AURA_UPPER:
                value = static_cast<float>(spell->m_cumulativeAura);
                break;

            case MATHOP_MAX_TARGET_LEVEL:
            case MATHOP_MAX_TARGET_LEVEL_UPPER:
                value = static_cast<float>(spell->m_maxTargetLevel);
                break;

            case MATHOP_AMPLITUDE_1:
            case MATHOP_AMPLITUDE_2:
            case MATHOP_AMPLITUDE_3:
            case MATHOP_AMPLITUDE_1_UPPER:
            case MATHOP_AMPLITUDE_2_UPPER:
            case MATHOP_AMPLITUDE_3_UPPER: {
                uint32_t index = op < MATHOP_AMPLITUDE_1_UPPER ? op - MATHOP_AMPLITUDE_1 : op - MATHOP_AMPLITUDE_1_UPPER;

                if (index < 3) {
                    float amplitude = spell->m_effectAmplitude[index];
                    Spell_C_ModifySpellValue(spell, amplitude, 27);
                    value = amplitude;
                }

                break;
            }

            case MATHOP_MAX_TARGETS:
            case MATHOP_MAX_TARGETS_UPPER:
                value = static_cast<float>(spell->m_maxTargets);
                break;

            case MATHOP_CHAIN_AMPLITUDE_1:
            case MATHOP_CHAIN_AMPLITUDE_2:
            case MATHOP_CHAIN_AMPLITUDE_3:
            case MATHOP_CHAIN_AMPLITUDE_1_UPPER:
            case MATHOP_CHAIN_AMPLITUDE_2_UPPER:
            case MATHOP_CHAIN_AMPLITUDE_3_UPPER: {
                uint32_t index = op < MATHOP_CHAIN_AMPLITUDE_1_UPPER ? op - MATHOP_CHAIN_AMPLITUDE_1 : op - MATHOP_CHAIN_AMPLITUDE_1_UPPER;

                if (index < 3) {
                    value = spell->m_effectChainAmplitude[index];
                }

                break;
            }

            case MATHOP_MISC_VALUE_1:
            case MATHOP_MISC_VALUE_2:
            case MATHOP_MISC_VALUE_3:
            case MATHOP_MISC_VALUE_1_UPPER:
            case MATHOP_MISC_VALUE_2_UPPER:
            case MATHOP_MISC_VALUE_3_UPPER: {
                uint32_t index = op < MATHOP_CHAIN_AMPLITUDE_1_UPPER ? op - MATHOP_CHAIN_AMPLITUDE_1 : op - MATHOP_CHAIN_AMPLITUDE_1_UPPER;

                if (index < 3) {
                    value = static_cast<float>(spell->m_effectMiscValue[index]);
                }

                break;
            }

            case MATHOP_ATTACK_POWER:
                value = static_cast<float>(player->m_unit->m_unitFieldAttackPower);
                value = 0.0f >= value ? 0.0f : value;
                break;

            case MATHOP_ATTACK_POWER_EFFECTIVE: {
                CGUnitData* data = player->m_unit;
                value = static_cast<float>(data->m_unitFieldAttackPower + data->m_unitFieldAttackPowerMods[0] + data->m_unitFieldAttackPowerMods[1]) * (data->m_unitFieldAttackPowerMultiplier + 1.0f);
                value = 0.0f >= value ? 0.0f : value;
                break;
            }

            case MATHOP_RANGED_ATTACK_POWER:
                value = static_cast<float>(player->m_unit->m_unitFieldRangedAttackPower);
                value = 0.0f >= value ? 0.0f : value;
                break;

            case MATHOP_RANGED_ATTACK_POWER_EFFECTIVE: {
                CGUnitData* data = player->m_unit;
                value = static_cast<float>(data->m_unitFieldRangedAttackPower + data->m_unitFieldRangedAttackPowerMods[0] + data->m_unitFieldRangedAttackPowerMods[1]) * (data->m_unitFieldRangedAttackPowerMultiplier + 1.0f);
                value = 0.0f >= value ? 0.0f : value;
                break;
            }

            case MATHOP_MAIN_HAND_WEAPON_MIN:
            case MATHOP_MAIN_HAND_WEAPON_MAX:
            case MATHOP_OFF_HAND_WEAPON_MIN:
            case MATHOP_OFF_HAND_WEAPON_MAX:
            case MATHOP_RANGED_WEAPON_MIN:
            case MATHOP_RANGED_WEAPON_MAX: {
                const VisibleItem_C* item = player->GetVisibleItem(op < MATHOP_OFF_HAND_WEAPON_MIN ? 15 : op < MATHOP_RANGED_WEAPON_MIN ? 16 : 17);

                if (!item) {
                    break;
                }

                ItemStats_C* stats = g_itemDBCache.GetRecord(abs(item->m_itemId), WOWGUID{ 0 }, nullptr, nullptr, false);

                if (!stats || stats->m_damageType[0]) {
                    break;
                }

                value = (op & 1) ? stats->m_damageMax[0] : stats->m_damageMin[0];
                break;
            }

            case MATHOP_MAIN_HAND_DAMAGE_MIN:
            case MATHOP_OFF_HAND_DAMAGE_MIN:
            case MATHOP_RANGED_DAMAGE_MIN: {
                float damage = op == MATHOP_MAIN_HAND_DAMAGE_MIN ? player->m_unit->m_unitFieldMindamage : op == MATHOP_OFF_HAND_DAMAGE_MIN ? player->m_unit->m_unitFieldMinoffhanddamage : player->m_unit->m_unitFieldMinrangeddamage;
                value = static_cast<float>(floor(damage));
                value = 1.0f >= value ? 1.0f : value;
                break;
            }

            case MATHOP_MAIN_HAND_DAMAGE_MAX:
            case MATHOP_OFF_HAND_DAMAGE_MAX:
            case MATHOP_RANGED_DAMAGE_MAX: {
                float damage = op == MATHOP_MAIN_HAND_DAMAGE_MAX ? player->m_unit->m_unitFieldMaxdamage : op == MATHOP_OFF_HAND_DAMAGE_MAX ? player->m_unit->m_unitFieldMaxoffhanddamage : player->m_unit->m_unitFieldMaxrangeddamage;
                value = ceilf(damage) > 1.0f ? ceilf(damage) : 1.0f;
                break;
            }

            case MATHOP_BASE_ARMOR: {
                int32_t baseValue = 0;
                int32_t total = 0;
                int32_t effective = 0;
                int32_t positive = 0;
                int32_t negative = 0;
                player->GetResistanceAndBuffs(GetPhysicalDamageClassID(), &baseValue, &total, &effective, &positive, &negative);
                value = static_cast<float>(baseValue);
                break;
            }

            case MATHOP_ARMOR: {
                int32_t baseValue = 0;
                int32_t total = 0;
                int32_t effective = 0;
                int32_t positive = 0;
                int32_t negative = 0;
                player->GetResistanceAndBuffs(GetPhysicalDamageClassID(), &baseValue, &total, &effective, &positive, &negative);
                value = static_cast<float>(effective);
                break;
            }

            case MATHOP_MAIN_HAND_SPEED:
            case MATHOP_MAIN_HAND_SPEED_UPPER:
                value = static_cast<float>(player->m_unit->m_unitFieldBaseattacktime[0]) * 0.001f;
                break;

            case MATHOP_OFF_HAND_SPEED:
            case MATHOP_OFF_HAND_SPEED_UPPER:
                value = static_cast<float>(player->m_unit->m_unitFieldBaseattacktime[1]) * 0.001f;
                break;

            case MATHOP_RANGED_SPEED:
            case MATHOP_RANGED_SPEED_UPPER:
                value = static_cast<float>(player->m_unit->m_unitFieldRangedattacktime) * 0.001f;
                break;

            case MATHOP_LEVEL:
            case MATHOP_LEVEL_UPPER:
                value = static_cast<float>(static_cast<int32_t>(player->m_unit->m_unitFieldLevel));
                break;

            case MATHOP_HANDS:
            case MATHOP_HANDS_UPPER:
                value = CGUnit_C::IsTwoHanded(player->GetVirtualItem(0, false)) ? 2.0f : 1.0f;
                break;

            case MATHOP_SPELL_POWER:
            case MATHOP_SPELL_POWER_UPPER: {
                int32_t bonus = player->GetModDamageDonePos(1) + player->GetModDamageDoneNeg(1);

                for (uint32_t school = 2; school <= 6; school++) {
                    if (bonus >= player->GetModDamageDonePos(school) + player->GetModDamageDoneNeg(school)) {
                        bonus = player->GetModDamageDonePos(school) + player->GetModDamageDoneNeg(school);
                    }
                }

                value = static_cast<float>(bonus);
                break;
            }

            case MATHOP_SPELL_POWER_HOLY:
            case MATHOP_SPELL_POWER_FIRE:
            case MATHOP_SPELL_POWER_NATURE:
            case MATHOP_SPELL_POWER_FROST:
            case MATHOP_SPELL_POWER_SHADOW:
            case MATHOP_SPELL_POWER_ARCANE:
            case MATHOP_SPELL_POWER_HOLY_UPPER:
            case MATHOP_SPELL_POWER_FIRE_UPPER:
            case MATHOP_SPELL_POWER_NATURE_UPPER:
            case MATHOP_SPELL_POWER_FROST_UPPER:
            case MATHOP_SPELL_POWER_SHADOW_UPPER:
            case MATHOP_SPELL_POWER_ARCANE_UPPER: {
                int32_t school = (op < MATHOP_SPELL_POWER_HOLY_UPPER ? op - MATHOP_SPELL_POWER_HOLY : op - MATHOP_SPELL_POWER_HOLY_UPPER) + 1;
                float positive = static_cast<float>(player->GetModDamageDonePos(school));
                value = static_cast<float>(player->GetModDamageDoneNeg(school)) + positive;
                break;
            }

            case MATHOP_HEALING_POWER:
            case MATHOP_HEALING_POWER_UPPER:
                value = static_cast<float>(static_cast<int32_t>(player->m_player->m_playerFieldModHealingDonePos));
                break;

            case MATHOP_DAMAGE_PCT_HOLY:
            case MATHOP_DAMAGE_PCT_FIRE:
            case MATHOP_DAMAGE_PCT_NATURE:
            case MATHOP_DAMAGE_PCT_FROST:
            case MATHOP_DAMAGE_PCT_SHADOW:
            case MATHOP_DAMAGE_PCT_ARCANE:
            case MATHOP_DAMAGE_PCT_HOLY_UPPER:
            case MATHOP_DAMAGE_PCT_FIRE_UPPER:
            case MATHOP_DAMAGE_PCT_NATURE_UPPER:
            case MATHOP_DAMAGE_PCT_FROST_UPPER:
            case MATHOP_DAMAGE_PCT_SHADOW_UPPER:
            case MATHOP_DAMAGE_PCT_ARCANE_UPPER:
                if (op < MATHOP_DAMAGE_PCT_HOLY_UPPER) {
                    value = player->GetModDamageDonePct(op - MATHOP_DAMAGE_PCT_HOLY + 1);
                } else {
                    value = player->GetModDamageDonePct(op - MATHOP_DAMAGE_PCT_HOLY_UPPER + 1);
                }

                break;

            case MATHOP_HEALING_PCT:
            case MATHOP_HEALING_PCT_UPPER:
                value = player->m_player->m_playerFieldModHealingPct;
                break;

            case MATHOP_HEALING_DONE_PCT:
            case MATHOP_HEALING_DONE_PCT_UPPER:
                value = player->m_player->m_playerFieldModHealingDonePct;
                break;

            case MATHOP_BONUS_COEFFICIENT_1:
            case MATHOP_BONUS_COEFFICIENT_2:
            case MATHOP_BONUS_COEFFICIENT_3:
            case MATHOP_BONUS_COEFFICIENT_1_UPPER:
            case MATHOP_BONUS_COEFFICIENT_2_UPPER:
            case MATHOP_BONUS_COEFFICIENT_3_UPPER: {
                uint32_t index = op < MATHOP_BONUS_COEFFICIENT_1_UPPER ? op - MATHOP_BONUS_COEFFICIENT_1 : op - MATHOP_BONUS_COEFFICIENT_1_UPPER;

                if (index < 3) {
                    float coefficient = spell->m_effectBonusCoefficient[index];
                    Spell_C_ModifySpellValue(spell, coefficient, 24);
                    value = coefficient;
                }

                break;
            }

            case MATHOP_EXT_HP:
                value = static_cast<float>(player->m_unit->m_unitFieldHealth);
                break;

            case MATHOP_EXT_HP_UPPER:
                value = static_cast<float>(player->m_unit->m_unitFieldMaxhealth);
                break;

            case MATHOP_EXT_MASTERY_1:
            case MATHOP_EXT_MASTERY_2:
            case MATHOP_EXT_MASTERY_3:
            case MATHOP_EXT_MASTERY_4:
                value = Duskhaven::Character::GetMasteryForSpec(op - MATHOP_EXT_MASTERY_1);
                break;

            case MATHOP_EXT_MASTERY:
                value = Duskhaven::Character::GetMasteryRatingSpec(Duskhaven::Character::SpecToIndex(Duskhaven::Character::GetCharActiveSpec()));
                break;

            case MATHOP_EXT_DPCT:
                value = player->m_player->m_playerDodgePercentage;
                break;

            case MATHOP_EXT_PPCT:
                value = player->m_player->m_playerParryPercentage;
                break;

            default:
                if (op >= MATHOP_EXT_PPL_1 && op <= MATHOP_EXT_PPL_32) {
                    uint32_t effectIdx = op - MATHOP_EXT_PPL_1;
                    SpellEffectCacheRow* customEffect = spell ? SpellEffectCacheDB::Get(SpellEffectCacheKey(spell->m_id, effectIdx)) : nullptr;
                    if (customEffect) {
                        value = customEffect->effectRealPointsPerLevel;
                    } else if (effectIdx < 3 && spell) {
                        value = spell->m_effectRealPointsPerLevel[effectIdx];
                    } else {
                        this->m_status = 1;
                        value = 0.0f;
                    }
                } else if (op >= MATHOP_EXT_PPL_1_UPPER && op <= MATHOP_EXT_PPL_32_UPPER) {
                    uint32_t effectIdx = op - MATHOP_EXT_PPL_1_UPPER;
                    SpellEffectCacheRow* customEffect = spell ? SpellEffectCacheDB::Get(SpellEffectCacheKey(spell->m_id, effectIdx)) : nullptr;
                    if (customEffect) {
                        value = customEffect->effectRealPointsPerLevel * static_cast<float>(player->m_unit->m_unitFieldLevel);
                    } else if (effectIdx < 3 && spell) {
                        value = spell->m_effectRealPointsPerLevel[effectIdx] * static_cast<float>(player->m_unit->m_unitFieldLevel);
                    } else {
                        this->m_status = 1;
                        value = 0.0f;
                    }
                } else if (op >= MATHOP_EXT_POWER_1 && op <= MATHOP_EXT_POWER_7) {
                    uint32_t powerIdx = op - MATHOP_EXT_POWER_1;
                    value = static_cast<float>(player->m_unit->m_unitFieldPower[powerIdx]);
                } else if (op >= MATHOP_EXT_POWER_1_UPPER && op <= MATHOP_EXT_POWER_7_UPPER) {
                    uint32_t powerIdx = op - MATHOP_EXT_POWER_1_UPPER;
                    value = static_cast<float>(player->m_unit->m_unitFieldMaxpower[powerIdx]);
                } else if (op >= MATHOP_EXT_BON_1 && op <= MATHOP_EXT_BON_32) {
                    uint32_t effectIdx = op - MATHOP_EXT_BON_1;
                    value = Duskhaven::Tooltip::GetEffectBonusValue(player, spell, effectIdx, spellLevel, parsePet, parseUnit, noModifiers);
                } else {
                    value = 0.0f;
                    this->m_status = 1;
                }
                break;
        }
    }

    stack->m_top--;
    stack->m_values[stack->m_top] = value;
}

// VERIFY: 1749bc57 manual
// OFFSET: 0x00578D10
float MathState::Evaluate(const SpellRec* spell, int32_t spellLevel, int32_t scale, int32_t parseUnit, int32_t parsePet, int32_t noModifiers) {
    MathStack stack;
    stack.m_top = 32;
    int32_t useLoaded = 0;
    SpellRec loaded;
    const float* constant = this->m_constants;
    const float* variable = this->m_variables;
    const int32_t* intArg = this->m_intArgs;

    for (uint32_t i = 0; i < this->m_numOps; i++) {
        uint16_t op = this->m_ops[i];

        if (op == MATHOP_SPELL) {
            if (!g_spellDB.GetRecord(QuestTextParser::Spell_C_SpellGetSpellIDForDifficulty(*intArg++), &loaded)) {
                this->m_status = 1;
                return 0.0f;
            }

            useLoaded = 1;
            op = this->m_ops[++i];
        }

        switch (op) {
            case MATHOP_CONSTANT:
                stack.m_values[--stack.m_top] = *constant++;
                break;

            case MATHOP_SPELL_VARIABLE:
                stack.m_values[--stack.m_top] = *variable++;
                break;

            case MATHOP_POW:
            case MATHOP_NEGATE:
            case MATHOP_MUL:
            case MATHOP_DIV:
            case MATHOP_MOD:
            case MATHOP_ADD:
            case MATHOP_SUB:
                this->ExecuteOperator(op, &stack);
                break;

            case MATHOP_ABS:
            case MATHOP_CEIL:
            case MATHOP_FLOOR:
            case MATHOP_MIN:
            case MATHOP_MAX:
            case MATHOP_GT:
            case MATHOP_LT:
            case MATHOP_GTE:
            case MATHOP_LTE:
            case MATHOP_EQ:
            case MATHOP_COND:
            case MATHOP_CLAMP:
                this->ExecuteFunction(op, &stack);
                break;

            default:
                this->GetVariableValue(op, &stack, useLoaded ? &loaded : spell, spellLevel, scale, parseUnit, parsePet, noModifiers);
                useLoaded = 0;
                break;
        }
    }

    float result = 0.0f;

    if (this->m_status == 2) {
        result = stack.m_values[stack.m_top];
    }

    return result;
}
