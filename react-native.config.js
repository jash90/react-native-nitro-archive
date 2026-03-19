module.exports = {
  dependency: {
    platforms: {
      ios: {},
      android: {
        sourceDir: 'android',
        packageImportPath: 'import com.margelo.nitro.archive.NitroArchiveOnLoad',
        cmakeListsPath: '../android/CMakeLists.txt',
      },
    },
  },
}
