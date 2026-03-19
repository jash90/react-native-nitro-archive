module.exports = {
  dependency: {
    platforms: {
      ios: {},
      android: {
        sourceDir: 'android',
        packageImportPath: 'import com.margelo.nitro.tarbz2.TarBz2OnLoad',
        cmakeListsPath: '../android/CMakeLists.txt',
      },
    },
  },
}
