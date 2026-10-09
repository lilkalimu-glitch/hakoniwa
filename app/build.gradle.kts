import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// Versionsnummer kommt vom Build auf GitHub (muss bei jedem Update steigen)
val appVersionCode = (findProperty("hakoniwa.versionCode") as String?)?.toInt() ?: 1
val appVersionName = (findProperty("hakoniwa.versionName") as String?) ?: "0.1-dev"

// Signierschlüssel (nur auf GitHub vorhanden). Ohne ihn wird mit dem Debug-Schlüssel signiert.
val keystorePath: String? = System.getenv("HAKONIWA_KEYSTORE")?.takeIf { it.isNotBlank() }

android {
    namespace = "com.lilkalimu.hakoniwa"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.lilkalimu.hakoniwa"
        minSdk = 31
        targetSdk = 35
        versionCode = appVersionCode
        versionName = appVersionName
    }

    signingConfigs {
        create("release") {
            if (keystorePath != null) {
                storeFile = file(keystorePath)
                storePassword = System.getenv("HAKONIWA_KEYSTORE_PASSWORD")
                keyAlias = System.getenv("HAKONIWA_KEY_ALIAS") ?: "hakoniwa"
                keyPassword = System.getenv("HAKONIWA_KEY_PASSWORD")
                    ?: System.getenv("HAKONIWA_KEYSTORE_PASSWORD")
            }
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
            signingConfig = if (keystorePath != null) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }
        }
    }

    // Eine APK pro Prozessor-Typ: arm64 fürs Handy, x86_64 für den Test-Emulator
    splits {
        abi {
            isEnable = true
            reset()
            include("arm64-v8a", "x86_64")
            isUniversalApk = false
        }
    }

    packaging {
        jniLibs {
            // libqemu.so ist ein Programm: Es muss als echte Datei entpackt werden,
            // damit die App es starten kann.
            useLegacyPackaging = true
            keepDebugSymbols += "**/libqemu.so"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    lint {
        checkReleaseBuilds = false
        abortOnError = false
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
    }
}
