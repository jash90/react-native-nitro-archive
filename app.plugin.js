// Expo Config Plugin for react-native-nitro-archive
// Injects Android native build configuration (CMake, C++ sources)

let configPlugins;
try {
  configPlugins = require('@expo/config-plugins');
} catch {
  configPlugins = require('expo/config-plugins');
}

const { withAppBuildGradle } = configPlugins;

function withNitroArchive(config) {
  return withAppBuildGradle(config, (config) => {
    if (config.modResults.contents.includes('react-native-nitro-archive')) {
      return config;
    }

    config.modResults.contents += `

// ===== react-native-nitro-archive Nitro Module =====
def nitroArchiveModule = new File(["node", "--print", "require.resolve('react-native-nitro-archive/package.json')"].execute(null, rootDir).text.trim()).parentFile
apply from: new File(nitroArchiveModule, 'nitrogen/generated/android/NitroArchive+autolinking.gradle')

android.sourceSets.main.java.srcDirs += [
    new File(nitroArchiveModule, 'android/src/main/java').absolutePath,
    new File(nitroArchiveModule, 'nitrogen/generated/android/kotlin').absolutePath,
]

afterEvaluate {
    tasks.matching { it.name.startsWith('configureCMake') }.configureEach { cmakeTask ->
        cmakeTask.doFirst {
            def autolinkCmake = file("\${buildDir}/generated/autolinking/src/main/jni/Android-autolinking.cmake")
            if (autolinkCmake.exists() && !autolinkCmake.text.contains('NitroArchive')) {
                def cmakeDir = new File(nitroArchiveModule, 'android').absolutePath
                def content = autolinkCmake.text
                content += "\\nadd_subdirectory(\\"" + cmakeDir + "\\" NitroArchive_autolinked_build)\\n"
                content = content.replace(
                    'set(AUTOLINKED_LIBRARIES',
                    'set(AUTOLINKED_LIBRARIES\\n  NitroArchive'
                )
                autolinkCmake.text = content
            }
        }
    }
}
// ===== end react-native-nitro-archive =====
`;

    return config;
  });
}

module.exports = withNitroArchive;
