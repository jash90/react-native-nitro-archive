// Expo Config Plugin for react-native-tar-bz2
// Injects Android native build configuration (Kotlin sources, CMake, Commons Compress)

let configPlugins;
try {
  configPlugins = require('@expo/config-plugins');
} catch {
  configPlugins = require('expo/config-plugins');
}

const { withAppBuildGradle } = configPlugins;

function withTarBz2(config) {
  return withAppBuildGradle(config, (config) => {
    if (config.modResults.contents.includes('react-native-tar-bz2')) {
      return config;
    }

    config.modResults.contents += `

// ===== react-native-tar-bz2 Nitro Module =====
def tarBz2Module = new File(["node", "--print", "require.resolve('react-native-tar-bz2/package.json')"].execute(null, rootDir).text.trim()).parentFile
apply from: new File(tarBz2Module, 'nitrogen/generated/android/TarBz2+autolinking.gradle')

android.sourceSets.main.java.srcDirs += [
    new File(tarBz2Module, 'android/src/main/java').absolutePath,
    new File(tarBz2Module, 'nitrogen/generated/android/kotlin').absolutePath,
]

dependencies {
    implementation 'org.apache.commons:commons-compress:1.26.0'
}

afterEvaluate {
    tasks.matching { it.name.startsWith('configureCMake') }.configureEach { cmakeTask ->
        cmakeTask.doFirst {
            def autolinkCmake = file("\${buildDir}/generated/autolinking/src/main/jni/Android-autolinking.cmake")
            if (autolinkCmake.exists() && !autolinkCmake.text.contains('TarBz2')) {
                def cmakeDir = new File(tarBz2Module, 'android').absolutePath
                def content = autolinkCmake.text
                content += "\\nadd_subdirectory(\\"" + cmakeDir + "\\" TarBz2_autolinked_build)\\n"
                content = content.replace(
                    'set(AUTOLINKED_LIBRARIES',
                    'set(AUTOLINKED_LIBRARIES\\n  TarBz2'
                )
                autolinkCmake.text = content
            }
        }
    }
}
// ===== end react-native-tar-bz2 =====
`;

    return config;
  });
}

module.exports = withTarBz2;
