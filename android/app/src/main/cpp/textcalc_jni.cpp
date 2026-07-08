#include <jni.h>

#include <string>
#include <vector>

#include "textcalc_c_api.h"

namespace {

jobject MakeResult(JNIEnv* env, bool ok, const std::string& output) {
    jclass result_class = env->FindClass("com/textcalculator/EvalResult");
    jmethodID constructor = env->GetMethodID(result_class, "<init>", "(ZLjava/lang/String;)V");
    jstring message = env->NewStringUTF(output.c_str());
    jobject result = env->NewObject(result_class, constructor, ok ? JNI_TRUE : JNI_FALSE, message);
    env->DeleteLocalRef(message);
    return result;
}

jobject MakeInsertion(JNIEnv* env, bool should_insert, const std::string& text) {
    jclass insertion_class = env->FindClass("com/textcalculator/EnterInsertion");
    jmethodID constructor = env->GetMethodID(insertion_class, "<init>", "(ZLjava/lang/String;)V");
    jstring insertion_text = env->NewStringUTF(text.c_str());
    jobject insertion =
        env->NewObject(insertion_class, constructor, should_insert ? JNI_TRUE : JNI_FALSE,
                       insertion_text);
    env->DeleteLocalRef(insertion_text);
    return insertion;
}

}  // namespace

extern "C" JNIEXPORT jobject JNICALL
Java_com_textcalculator_NativeCalculator_evaluate(JNIEnv* env, jclass, jstring expression) {
    if (expression == nullptr) {
        return MakeResult(env, false, "expression is null");
    }

    const char* input = env->GetStringUTFChars(expression, nullptr);
    if (input == nullptr) {
        return MakeResult(env, false, "failed to read expression");
    }

    TextCalcResult first_pass{};
    TextCalcStatus status = textcalc_evaluate_utf8(input, nullptr, 0, &first_pass);
    if (status != TEXTCALC_STATUS_OK) {
        env->ReleaseStringUTFChars(expression, input);
        return MakeResult(env, false, "invalid UTF-8 input");
    }

    std::vector<char> output(first_pass.required_size);
    TextCalcResult final_result{};
    status = textcalc_evaluate_utf8(input, output.data(), output.size(), &final_result);
    env->ReleaseStringUTFChars(expression, input);

    if (status != TEXTCALC_STATUS_OK) {
        return MakeResult(env, false, "evaluation failed");
    }

    return MakeResult(env, final_result.ok == 1, output.data());
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_textcalculator_NativeCalculator_buildEnterInsertion(JNIEnv* env,
                                                             jclass,
                                                             jstring text_before_caret) {
    if (text_before_caret == nullptr) {
        return MakeInsertion(env, false, "");
    }

    const char* input = env->GetStringUTFChars(text_before_caret, nullptr);
    if (input == nullptr) {
        return MakeInsertion(env, false, "");
    }

    TextCalcInsertion first_pass{};
    TextCalcStatus status = textcalc_build_enter_insertion_utf8(input, nullptr, 0, &first_pass);
    if (status != TEXTCALC_STATUS_OK) {
        env->ReleaseStringUTFChars(text_before_caret, input);
        return MakeInsertion(env, false, "");
    }

    std::vector<char> output(first_pass.required_size);
    TextCalcInsertion final_result{};
    status = textcalc_build_enter_insertion_utf8(input, output.data(), output.size(), &final_result);
    env->ReleaseStringUTFChars(text_before_caret, input);

    if (status != TEXTCALC_STATUS_OK) {
        return MakeInsertion(env, false, "");
    }

    return MakeInsertion(env, final_result.should_insert == 1, output.data());
}
