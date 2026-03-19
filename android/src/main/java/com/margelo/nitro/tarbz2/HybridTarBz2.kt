package com.margelo.nitro.tarbz2

import com.margelo.nitro.core.Promise
import org.apache.commons.compress.archivers.tar.TarArchiveEntry
import org.apache.commons.compress.archivers.tar.TarArchiveInputStream
import org.apache.commons.compress.archivers.tar.TarArchiveOutputStream
import org.apache.commons.compress.compressors.bzip2.BZip2CompressorInputStream
import org.apache.commons.compress.compressors.bzip2.BZip2CompressorOutputStream
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream

class HybridTarBz2 : HybridTarBz2Spec() {

  override fun unpack(
    sourcePath: String,
    targetPath: String,
    overwrite: Boolean
  ): Promise<TarBz2Result> {
    return Promise.async {
      performUnpack(sourcePath, targetPath, overwrite)
    }
  }

  private fun performUnpack(
    sourcePath: String,
    targetPath: String,
    overwrite: Boolean
  ): TarBz2Result {
    val targetDir = File(targetPath)
    if (!targetDir.exists()) {
      targetDir.mkdirs()
    }

    try {
      FileInputStream(sourcePath).use { fis ->
        BufferedInputStream(fis).use { bis ->
          BZip2CompressorInputStream(bis).use { bzis ->
            TarArchiveInputStream(bzis).use { tais ->
              var entry: TarArchiveEntry? = tais.nextEntry
              while (entry != null) {
                val outputFile = File(targetDir, entry.name)

                // Security: prevent zip-slip
                val canonicalTarget = targetDir.canonicalPath
                val canonicalOutput = outputFile.canonicalPath
                if (!canonicalOutput.startsWith(canonicalTarget)) {
                  return TarBz2Result(
                    success = false,
                    path = "",
                    error = "Entry outside target directory: ${entry.name}"
                  )
                }

                if (entry.isDirectory) {
                  outputFile.mkdirs()
                } else {
                  if (!overwrite && outputFile.exists()) {
                    entry = tais.nextEntry
                    continue
                  }
                  outputFile.parentFile?.mkdirs()
                  extractEntry(tais, outputFile)
                }
                entry = tais.nextEntry
              }
            }
          }
        }
      }
    } catch (e: Exception) {
      return TarBz2Result(success = false, path = "", error = e.message ?: "Unknown error")
    }

    return TarBz2Result(success = true, path = targetPath, error = "")
  }

  private fun extractEntry(input: TarArchiveInputStream, outputFile: File) {
    FileOutputStream(outputFile).use { fos ->
      val buffer = ByteArray(65536)
      var bytesRead: Int
      while (input.read(buffer).also { bytesRead = it } != -1) {
        fos.write(buffer, 0, bytesRead)
      }
    }
  }

  override fun pack(sourcePath: String, targetPath: String): Promise<TarBz2Result> {
    return Promise.async {
      performPack(sourcePath, targetPath)
    }
  }

  private fun performPack(sourcePath: String, targetPath: String): TarBz2Result {
    val sourceDir = File(sourcePath)
    if (!sourceDir.exists()) {
      return TarBz2Result(success = false, path = "", error = "Source path does not exist: $sourcePath")
    }

    try {
      FileOutputStream(targetPath).use { fos ->
        BufferedOutputStream(fos).use { bos ->
          BZip2CompressorOutputStream(bos).use { bzos ->
            TarArchiveOutputStream(bzos).use { taos ->
              taos.setLongFileMode(TarArchiveOutputStream.LONGFILE_POSIX)
              taos.setBigNumberMode(TarArchiveOutputStream.BIGNUMBER_POSIX)

              if (sourceDir.isDirectory) {
                packDirectory(taos, sourceDir, "")
              } else {
                packFile(taos, sourceDir, sourceDir.name)
              }

              taos.finish()
            }
          }
        }
      }
    } catch (e: Exception) {
      return TarBz2Result(success = false, path = "", error = e.message ?: "Unknown error")
    }

    return TarBz2Result(success = true, path = targetPath, error = "")
  }

  private fun packDirectory(
    taos: TarArchiveOutputStream,
    dir: File,
    basePath: String
  ) {
    val files = dir.listFiles() ?: return
    for (file in files) {
      val entryName = if (basePath.isEmpty()) file.name else "$basePath/${file.name}"
      if (file.isDirectory) {
        val entry = TarArchiveEntry(file, "$entryName/")
        taos.putArchiveEntry(entry)
        taos.closeArchiveEntry()
        packDirectory(taos, file, entryName)
      } else {
        packFile(taos, file, entryName)
      }
    }
  }

  private fun packFile(taos: TarArchiveOutputStream, file: File, entryName: String) {
    val entry = TarArchiveEntry(file, entryName)
    entry.size = file.length()
    taos.putArchiveEntry(entry)

    FileInputStream(file).use { fis ->
      val buffer = ByteArray(65536)
      var bytesRead: Int
      while (fis.read(buffer).also { bytesRead = it } != -1) {
        taos.write(buffer, 0, bytesRead)
      }
    }

    taos.closeArchiveEntry()
  }

  override fun listContents(sourcePath: String): Promise<Array<String>> {
    return Promise.async {
      performListContents(sourcePath)
    }
  }

  private fun performListContents(sourcePath: String): Array<String> {
    val entries = mutableListOf<String>()

    FileInputStream(sourcePath).use { fis ->
      BufferedInputStream(fis).use { bis ->
        BZip2CompressorInputStream(bis).use { bzis ->
          TarArchiveInputStream(bzis).use { tais ->
            var entry: TarArchiveEntry? = tais.nextEntry
            while (entry != null) {
              entries.add(entry.name)
              entry = tais.nextEntry
            }
          }
        }
      }
    }

    return entries.toTypedArray()
  }

  override fun unpackWithProgress(
    sourcePath: String,
    targetPath: String,
    overwrite: Boolean,
    onProgress: (progress: Double) -> Unit
  ): Promise<TarBz2Result> {
    return Promise.async {
      onProgress(0.0)
      val targetDir = File(targetPath)
      if (!targetDir.exists()) targetDir.mkdirs()

      try {
        val sourceFile = File(sourcePath)
        val totalBytes = sourceFile.length().toDouble()
        var bytesProcessed = 0L

        onProgress(0.1)

        FileInputStream(sourcePath).use { fis ->
          BufferedInputStream(fis).use { bis ->
            BZip2CompressorInputStream(bis).use { bzis ->
              TarArchiveInputStream(bzis).use { tais ->
                onProgress(0.3)
                var entry: TarArchiveEntry? = tais.nextEntry
                while (entry != null) {
                  val outputFile = File(targetDir, entry.name)
                  val canonicalTarget = targetDir.canonicalPath
                  val canonicalOutput = outputFile.canonicalPath
                  if (!canonicalOutput.startsWith(canonicalTarget)) {
                    return@async TarBz2Result(
                      success = false, path = "",
                      error = "Entry outside target directory: ${entry.name}"
                    )
                  }

                  if (entry.isDirectory) {
                    outputFile.mkdirs()
                  } else {
                    if (!overwrite && outputFile.exists()) {
                      entry = tais.nextEntry
                      continue
                    }
                    outputFile.parentFile?.mkdirs()
                    extractEntry(tais, outputFile)
                  }

                  bytesProcessed += entry.size
                  if (totalBytes > 0) {
                    onProgress(0.3 + 0.7 * (bytesProcessed / totalBytes).coerceAtMost(0.99))
                  }

                  entry = tais.nextEntry
                }
              }
            }
          }
        }
      } catch (e: Exception) {
        return@async TarBz2Result(success = false, path = "", error = e.message ?: "Unknown error")
      }

      onProgress(1.0)
      TarBz2Result(success = true, path = targetPath, error = "")
    }
  }

  override fun packWithProgress(
    sourcePath: String,
    targetPath: String,
    onProgress: (progress: Double) -> Unit
  ): Promise<TarBz2Result> {
    return Promise.async {
      val sourceDir = File(sourcePath)
      if (!sourceDir.exists()) {
        return@async TarBz2Result(success = false, path = "", error = "Source path does not exist: $sourcePath")
      }

      onProgress(0.0)

      try {
        val allFiles = if (sourceDir.isDirectory) {
          sourceDir.walkTopDown().toList()
        } else {
          listOf(sourceDir)
        }
        val totalFiles = allFiles.size.toDouble()
        var processedFiles = 0

        FileOutputStream(targetPath).use { fos ->
          BufferedOutputStream(fos).use { bos ->
            BZip2CompressorOutputStream(bos).use { bzos ->
              TarArchiveOutputStream(bzos).use { taos ->
                taos.setLongFileMode(TarArchiveOutputStream.LONGFILE_POSIX)
                taos.setBigNumberMode(TarArchiveOutputStream.BIGNUMBER_POSIX)

                if (sourceDir.isDirectory) {
                  packDirectory(taos, sourceDir, "")
                } else {
                  packFile(taos, sourceDir, sourceDir.name)
                }

                // Report progress per file (approximate)
                processedFiles = allFiles.size
                onProgress(0.9)

                taos.finish()
              }
            }
          }
        }
      } catch (e: Exception) {
        return@async TarBz2Result(success = false, path = "", error = e.message ?: "Unknown error")
      }

      onProgress(1.0)
      TarBz2Result(success = true, path = targetPath, error = "")
    }
  }
}
