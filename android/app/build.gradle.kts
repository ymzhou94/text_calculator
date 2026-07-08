plugins {
    id("com.android.application")
}

android {
    namespace = "com.textcalculator"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.textcalculator"
        minSdk = 23
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++20"
            }
        }

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
