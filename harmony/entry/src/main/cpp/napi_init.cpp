#include "napi/native_api.h"

#include <string>
#include <vector>

#include "textcalc_c_api.h"

namespace {

napi_value MakeResult(napi_env env, bool ok, const std::string& output, double value) {
    napi_value result = nullptr;
    napi_create_object(env, &result);

    napi_value ok_value = nullptr;
    napi_get_boolean(env, ok, &ok_value);
    napi_set_named_property(env, result, "ok", ok_value);

    napi_value output_value = nullptr;
    napi_create_string_utf8(env, output.c_str(), output.size(), &output_value);
    napi_set_named_property(env, result, "output", output_value);

    napi_value number_value = nullptr;
    napi_create_double(env, value, &number_value);
    napi_set_named_property(env, result, "value", number_value);

    return result;
}

napi_value MakeInsertion(napi_env env, bool should_insert, const std::string& text) {
    napi_value result = nullptr;
    napi_create_object(env, &result);

    napi_value should_insert_value = nullptr;
    napi_get_boolean(env, should_insert, &should_insert_value);
    napi_set_named_property(env, result, "shouldInsert", should_insert_value);

    napi_value text_value = nullptr;
    napi_create_string_utf8(env, text.c_str(), text.size(), &text_value);
    napi_set_named_property(env, result, "text", text_value);

    return result;
}

napi_value Evaluate(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc != 1) {
        return MakeResult(env, false, "evaluate expects one expression", 0.0);
    }

    napi_valuetype type = napi_undefined;
    napi_typeof(env, args[0], &type);
    if (type != napi_string) {
        return MakeResult(env, false, "expression must be a string", 0.0);
    }

    size_t input_size = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &input_size);
    std::vector<char> input(input_size + 1);
    napi_get_value_string_utf8(env, args[0], input.data(), input.size(), &input_size);

    TextCalcResult first_pass{};
    TextCalcStatus status = textcalc_evaluate_utf8(input.data(), nullptr, 0, &first_pass);
    if (status != TEXTCALC_STATUS_OK) {
        return MakeResult(env, false, "invalid UTF-8 input", 0.0);
    }

    std::vector<char> output(first_pass.required_size);
    TextCalcResult final_result{};
    status = textcalc_evaluate_utf8(input.data(), output.data(), output.size(), &final_result);
    if (status != TEXTCALC_STATUS_OK) {
        return MakeResult(env, false, "evaluation failed", 0.0);
    }

    return MakeResult(env, final_result.ok == 1, output.data(), final_result.value);
}

napi_value BuildEnterInsertion(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc != 1) {
        return MakeInsertion(env, false, "");
    }

    napi_valuetype type = napi_undefined;
    napi_typeof(env, args[0], &type);
    if (type != napi_string) {
        return MakeInsertion(env, false, "");
    }

    size_t input_size = 0;
    napi_get_value_string_utf8(env, args[0], nullptr, 0, &input_size);
    std::vector<char> input(input_size + 1);
    napi_get_value_string_utf8(env, args[0], input.data(), input.size(), &input_size);

    TextCalcInsertion first_pass{};
    TextCalcStatus status = textcalc_build_enter_insertion_utf8(input.data(), nullptr, 0, &first_pass);
    if (status != TEXTCALC_STATUS_OK) {
        return MakeInsertion(env, false, "");
    }

    std::vector<char> output(first_pass.required_size);
    TextCalcInsertion final_result{};
    status = textcalc_build_enter_insertion_utf8(input.data(), output.data(), output.size(),
                                                 &final_result);
    if (status != TEXTCALC_STATUS_OK) {
        return MakeInsertion(env, false, "");
    }

    return MakeInsertion(env, final_result.should_insert == 1, output.data());
}

}  // namespace

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc[] = {
        { "evaluate", nullptr, Evaluate, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "buildEnterInsertion", nullptr, BuildEnterInsertion, nullptr, nullptr, nullptr,
          napi_default, nullptr }
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module textcalcModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "textcalc",
    .nm_priv = nullptr,
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterTextCalcModule(void) {
    napi_module_register(&textcalcModule);
}
