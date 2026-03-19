import { StatusBar } from 'expo-status-bar';
import { useRef, useState } from 'react';
import {
  Platform,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import { File, Directory, Paths } from 'expo-file-system';
import {
  pack,
  unpack,
  listContents,
  detectFormat,
  testArchive,
} from 'react-native-nitro-archive';
import { SafeAreaView } from 'react-native-safe-area-context';

/** Strip file:// prefix to get a plain filesystem path */
const toPath = (f: { uri: string }) => f.uri.replace(/^file:\/\//, '');

const TEST_DIR = new Directory(Paths.cache, 'archive-test');
const SOURCE_DIR = new Directory(TEST_DIR, 'source');
const SUBDIR = new Directory(SOURCE_DIR, 'subdir');
const EMPTY_DIR = new Directory(SOURCE_DIR, 'empty-dir');

// tar.bz2
const ARCHIVE_BZ2 = new File(TEST_DIR, 'test.tar.bz2');
const UNPACK_BZ2 = new Directory(TEST_DIR, 'unpacked-bz2');

// tar.gz
const ARCHIVE_GZ = new File(TEST_DIR, 'test.tar.gz');
const UNPACK_GZ = new Directory(TEST_DIR, 'unpacked-gz');

// plain tar
const ARCHIVE_TAR = new File(TEST_DIR, 'test.tar');
const UNPACK_TAR = new Directory(TEST_DIR, 'unpacked-tar');

// plain bz2 (single file)
const SINGLE_FILE = new File(TEST_DIR, 'single.txt');
const ARCHIVE_PLAIN_BZ2 = new File(TEST_DIR, 'single.txt.bz2');
const UNPACK_PLAIN_BZ2 = new Directory(TEST_DIR, 'unpacked-plain-bz2');

// plain gz (single file)
const ARCHIVE_PLAIN_GZ = new File(TEST_DIR, 'single.txt.gz');
const UNPACK_PLAIN_GZ = new Directory(TEST_DIR, 'unpacked-plain-gz');

// Progress test
const LARGE_DIR = new Directory(TEST_DIR, 'large-source');
const LARGE_ARCHIVE = new File(TEST_DIR, 'large.tar.bz2');
const LARGE_UNPACK_DIR = new Directory(TEST_DIR, 'large-unpacked');

// ZIP
const ARCHIVE_ZIP = new File(TEST_DIR, 'test.zip');
const UNPACK_ZIP = new Directory(TEST_DIR, 'unpacked-zip');
const ARCHIVE_ZIP_PWD = new File(TEST_DIR, 'test-encrypted.zip');
const UNPACK_ZIP_PWD = new Directory(TEST_DIR, 'unpacked-zip-pwd');

// 7z
const ARCHIVE_7Z = new File(TEST_DIR, 'test.7z');
const UNPACK_7Z = new Directory(TEST_DIR, 'unpacked-7z');

// Base64-encoded minimal .7z containing hello7z.txt and polish7z.txt
const TEST_7Z_BASE64 =
  'N3q8ryccAAS1itr+mAAAAAAAAAAhAAAAAAAAAKyPBRYBAClIZWxsbyBmcm9tIDd6IQpaYcW8' +
  'w7PFgsSHIGfEmcWbbMSFIGphxbrFhAoAAACBMweuD9Dij7yfP0dBBA9xDP25DwI9cYWbMfPs' +
  'OEn+XVjrP03T/ZgbIBwpg+MywcV0m/eW7BV3E8BEKLfcl23h2uaRF7f2AIBtbuTspkwmf05B' +
  '3pPrEg/8qIaHCMhPbvaZBc23AAAAABcGLgEJagAHCwEAASMDAQEFXQAQAAAMgIYKAT3ZqS8AAA==';

type LogEntry = { text: string; ok: boolean };

export default function App() {
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [running, setRunning] = useState(false);
  const [progressBar, setProgressBar] = useState(0);
  const scrollRef = useRef<ScrollView>(null);

  const log = (text: string, ok = true) => {
    setLogs((prev) => [...prev, { text, ok }]);
    setTimeout(() => scrollRef.current?.scrollToEnd({ animated: false }), 50);
  };

  const setupTestFiles = () => {
    SOURCE_DIR.create();
    SUBDIR.create();
    EMPTY_DIR.create();

    const hello = new File(SOURCE_DIR, 'hello.txt');
    hello.create();
    hello.write('Hello, nitro-archive!');

    const polish = new File(SOURCE_DIR, 'polskie-znaki.txt');
    polish.create();
    polish.write('Zażółć gęślą jaźń — łódź');

    const nested = new File(SUBDIR, 'nested.txt');
    nested.create();
    nested.write('Nested file content');
  };

  const setupLargeFiles = () => {
    LARGE_DIR.create();
    const chunk = 'abcdefghij'.repeat(100);
    for (let i = 0; i < 10; i++) {
      const f = new File(LARGE_DIR, `file-${String(i).padStart(3, '0')}.txt`);
      f.create();
      f.write(chunk);
    }
  };

  const cleanup = () => {
    try {
      if (TEST_DIR.exists) TEST_DIR.delete();
    } catch { }
  };

  const verifyRoundTrip = (name: string, orig: File, unpacked: File) => {
    const a = orig.textSync();
    const b = unpacked.textSync();
    if (a === b) {
      log(`✅ ${name} matches`);
    } else {
      log(`❌ ${name} mismatch!`, false);
    }
  };

  const runTests = async () => {
    setRunning(true);
    setLogs([]);
    setProgressBar(0);

    const srcPath = toPath(SOURCE_DIR);

    try {
      cleanup();
      TEST_DIR.create();
      log('🧹 Cleanup done');
      setupTestFiles();
      log('📁 Test files created');

      // ===== 1. PACK tar.bz2 =====
      log('');
      log('━━━ 1. Pack tar.bz2 ━━━');
      const bz2Path = toPath(ARCHIVE_BZ2);
      const packBz2 = await pack(srcPath, bz2Path, 'tar.bz2');
      log(packBz2.success ? '✅ Pack tar.bz2 OK' : `❌ Pack FAILED: ${packBz2.errorMessage}`, packBz2.success);

      // ===== 2. Detect format =====
      log('');
      log('━━━ 2. Detect format ━━━');
      const detected = await detectFormat(bz2Path);
      if (detected === 'tar.bz2') {
        log('✅ detectFormat = tar.bz2');
      } else {
        log(`❌ Expected tar.bz2, got: ${detected}`, false);
      }

      // ===== 3. List contents tar.bz2 =====
      log('');
      log('━━━ 3. List contents tar.bz2 ━━━');
      const contents = await listContents(bz2Path);
      log(`📋 Entries (${contents.length}):`);
      contents.forEach((e) => log(`   • ${e.path} ${e.isDirectory ? '(dir)' : `(${e.size}B)`}`));

      // ===== 4. Unpack tar.bz2 =====
      log('');
      log('━━━ 4. Unpack tar.bz2 ━━━');
      UNPACK_BZ2.create();
      const unpackBz2 = await unpack(bz2Path, toPath(UNPACK_BZ2), true);
      log(unpackBz2.success ? '✅ Unpack tar.bz2 OK' : `❌ Unpack FAILED: ${unpackBz2.errorMessage}`, unpackBz2.success);

      // ===== 5. Round-trip verification (bz2) =====
      log('');
      log('━━━ 5. Round-trip verification (bz2) ━━━');
      verifyRoundTrip('hello.txt', new File(SOURCE_DIR, 'hello.txt'), new File(UNPACK_BZ2, 'hello.txt'));
      verifyRoundTrip('polskie-znaki.txt', new File(SOURCE_DIR, 'polskie-znaki.txt'), new File(UNPACK_BZ2, 'polskie-znaki.txt'));
      verifyRoundTrip('subdir/nested.txt', new File(SUBDIR, 'nested.txt'), new File(new Directory(UNPACK_BZ2, 'subdir'), 'nested.txt'));

      // ===== 6. PACK tar.gz =====
      log('');
      log('━━━ 6. Pack tar.gz ━━━');
      const gzPath = toPath(ARCHIVE_GZ);
      const packGz = await pack(srcPath, gzPath, 'tar.gz');
      log(packGz.success ? '✅ Pack tar.gz OK' : `❌ Pack FAILED: ${packGz.errorMessage}`, packGz.success);

      // Detect tar.gz
      const detectedGz = await detectFormat(gzPath);
      if (detectedGz === 'tar.gz') {
        log('✅ detectFormat = tar.gz');
      } else {
        log(`❌ Expected tar.gz, got: ${detectedGz}`, false);
      }

      // ===== 7. Unpack tar.gz =====
      log('');
      log('━━━ 7. Unpack tar.gz ━━━');
      UNPACK_GZ.create();
      const unpackGz = await unpack(gzPath, toPath(UNPACK_GZ), true);
      log(unpackGz.success ? '✅ Unpack tar.gz OK' : `❌ Unpack FAILED: ${unpackGz.errorMessage}`, unpackGz.success);

      // Round-trip gz
      verifyRoundTrip('hello.txt (gz)', new File(SOURCE_DIR, 'hello.txt'), new File(UNPACK_GZ, 'hello.txt'));
      verifyRoundTrip('polskie-znaki.txt (gz)', new File(SOURCE_DIR, 'polskie-znaki.txt'), new File(UNPACK_GZ, 'polskie-znaki.txt'));

      // ===== 8. PACK plain tar =====
      log('');
      log('━━━ 8. Pack plain tar ━━━');
      const tarPath = toPath(ARCHIVE_TAR);
      const packTar = await pack(srcPath, tarPath, 'tar');
      log(packTar.success ? '✅ Pack tar OK' : `❌ Pack FAILED: ${packTar.errorMessage}`, packTar.success);

      const detectedTar = await detectFormat(tarPath);
      if (detectedTar === 'tar') {
        log('✅ detectFormat = tar');
      } else {
        log(`❌ Expected tar, got: ${detectedTar}`, false);
      }

      // List & unpack tar
      const tarContents = await listContents(tarPath);
      log(`📋 tar entries: ${tarContents.length}`);

      UNPACK_TAR.create();
      const unpackTar = await unpack(tarPath, toPath(UNPACK_TAR), true);
      log(unpackTar.success ? '✅ Unpack tar OK' : `❌ Unpack FAILED: ${unpackTar.errorMessage}`, unpackTar.success);
      verifyRoundTrip('hello.txt (tar)', new File(SOURCE_DIR, 'hello.txt'), new File(UNPACK_TAR, 'hello.txt'));
      verifyRoundTrip('subdir/nested.txt (tar)', new File(SUBDIR, 'nested.txt'), new File(new Directory(UNPACK_TAR, 'subdir'), 'nested.txt'));

      // ===== 9. PACK plain bz2 (single file) =====
      log('');
      log('━━━ 9. Pack plain bz2 (single file) ━━━');
      SINGLE_FILE.create();
      SINGLE_FILE.write('Compress me with bzip2!');
      const singlePath = toPath(SINGLE_FILE);
      const plainBz2Path = toPath(ARCHIVE_PLAIN_BZ2);
      const packPlainBz2 = await pack(singlePath, plainBz2Path, 'bz2');
      log(packPlainBz2.success ? '✅ Pack bz2 OK' : `❌ Pack FAILED: ${packPlainBz2.errorMessage}`, packPlainBz2.success);

      const detectedPlainBz2 = await detectFormat(plainBz2Path);
      log(`   detectFormat = ${detectedPlainBz2}`);

      // ===== 10. PACK plain gz (single file) =====
      log('');
      log('━━━ 10. Pack plain gz (single file) ━━━');
      const plainGzPath = toPath(ARCHIVE_PLAIN_GZ);
      const packPlainGz = await pack(singlePath, plainGzPath, 'gz');
      log(packPlainGz.success ? '✅ Pack gz OK' : `❌ Pack FAILED: ${packPlainGz.errorMessage}`, packPlainGz.success);

      const detectedPlainGz = await detectFormat(plainGzPath);
      log(`   detectFormat = ${detectedPlainGz}`);

      // ===== 11. Overwrite=false =====
      log('');
      log('━━━ 11. Overwrite flag ━━━');
      const modFile = new File(UNPACK_BZ2, 'hello.txt');
      modFile.write('MODIFIED');
      await unpack(bz2Path, toPath(UNPACK_BZ2), false);
      if (modFile.textSync() === 'MODIFIED') {
        log('✅ overwrite=false preserved existing file');
      } else {
        log('❌ overwrite=false did NOT preserve file!', false);
      }

      // ===== 12. Error handling =====
      log('');
      log('━━━ 12. Error handling ━━━');
      const badUnpack = await unpack('/nonexistent.tar.bz2', toPath(UNPACK_BZ2), true);
      if (!badUnpack.success && badUnpack.errorMessage.length > 0) {
        log(`✅ Bad unpack: ${badUnpack.errorMessage}`);
      } else {
        log('❌ Should have failed on nonexistent file!', false);
      }

      const badPack = await pack('/nonexistent-dir', bz2Path, 'tar.bz2');
      if (!badPack.success && badPack.errorMessage.length > 0) {
        log(`✅ Bad pack: ${badPack.errorMessage}`);
      } else {
        log('❌ Should have failed on nonexistent source!', false);
      }

      // ===== 13. Pack with progress =====
      log('');
      log('━━━ 13. Pack with progress callback ━━━');
      setupLargeFiles();

      const progressVals: number[] = [];
      setProgressBar(0);
      const packProgress = await pack(
        toPath(LARGE_DIR),
        toPath(LARGE_ARCHIVE),
        'tar.bz2',
        undefined,
        (bytesProcessed, totalBytes) => {
          const p = totalBytes > 0 ? bytesProcessed / totalBytes : 0;
          progressVals.push(p);
          setProgressBar(p);
        }
      );
      log(packProgress.success ? '✅ Pack with progress OK' : `❌ FAILED: ${packProgress.errorMessage}`, packProgress.success);

      if (progressVals.length >= 2) {
        log(`✅ Progress callbacks: ${progressVals.length}`);
      } else {
        log(`❌ Too few callbacks: ${progressVals.length}`, false);
      }

      // ===== 14. Unpack with progress =====
      log('');
      log('━━━ 14. Unpack with progress ━━━');
      LARGE_UNPACK_DIR.create();
      const unpackProgress = await unpack(
        toPath(LARGE_ARCHIVE),
        toPath(LARGE_UNPACK_DIR),
        true,
        undefined,
        (bytesProcessed, totalBytes) => {
          const p = totalBytes > 0 ? bytesProcessed / totalBytes : 0;
          setProgressBar(p);
        }
      );
      log(unpackProgress.success ? '✅ Unpack with progress OK' : `❌ FAILED: ${unpackProgress.errorMessage}`, unpackProgress.success);

      // Verify large round-trip
      for (const idx of [0, 4, 9]) {
        const name = `file-${String(idx).padStart(3, '0')}.txt`;
        const orig = new File(LARGE_DIR, name).textSync();
        const unpacked = new File(LARGE_UNPACK_DIR, name).textSync();
        if (orig === unpacked) {
          log(`✅ Large round-trip: ${name} matches`);
        } else {
          log(`❌ Large round-trip: ${name} mismatch!`, false);
        }
      }

      // ===== 15. ZIP pack → detect → list → unpack → round-trip =====
      log('');
      log('━━━ 15. ZIP pack → round-trip ━━━');
      const zipPath = toPath(ARCHIVE_ZIP);
      const packZip = await pack(srcPath, zipPath, 'zip');
      log(packZip.success ? '✅ Pack zip OK' : `❌ Pack zip FAILED: ${packZip.errorMessage}`, packZip.success);

      const detectedZip = await detectFormat(zipPath);
      if (detectedZip === 'zip') {
        log('✅ detectFormat = zip');
      } else {
        log(`❌ Expected zip, got: ${detectedZip}`, false);
      }

      const zipContents = await listContents(zipPath);
      log(`📋 ZIP entries (${zipContents.length}):`);
      zipContents.forEach((e) =>
        log(`   • ${e.path} ${e.isDirectory ? '(dir)' : `(${e.size}B, compressed: ${e.compressedSize}B)`} ${e.isEncrypted ? '🔒' : ''}`)
      );

      UNPACK_ZIP.create();
      const unpackZip = await unpack(zipPath, toPath(UNPACK_ZIP), true);
      log(unpackZip.success ? '✅ Unpack zip OK' : `❌ Unpack zip FAILED: ${unpackZip.errorMessage}`, unpackZip.success);

      verifyRoundTrip('hello.txt (zip)', new File(SOURCE_DIR, 'hello.txt'), new File(UNPACK_ZIP, 'hello.txt'));
      verifyRoundTrip('polskie-znaki.txt (zip)', new File(SOURCE_DIR, 'polskie-znaki.txt'), new File(UNPACK_ZIP, 'polskie-znaki.txt'));

      // ===== 16. ZIP with password (AES-256) =====
      log('');
      log('━━━ 16. ZIP with password (AES-256) ━━━');
      const zipPwdPath = toPath(ARCHIVE_ZIP_PWD);
      const packZipPwd = await pack(srcPath, zipPwdPath, 'zip', { password: 'secret123' });
      log(packZipPwd.success ? '✅ Pack encrypted zip OK' : `❌ Pack FAILED: ${packZipPwd.errorMessage}`, packZipPwd.success);

      // List encrypted zip — check isEncrypted
      const encContents = await listContents(zipPwdPath);
      const hasEncrypted = encContents.some((e) => e.isEncrypted && !e.isDirectory);
      if (hasEncrypted) {
        log('✅ Encrypted entries detected in listing');
      } else {
        log('❌ Expected encrypted entries!', false);
      }

      // Unpack with correct password
      UNPACK_ZIP_PWD.create();
      const unpackZipPwd = await unpack(zipPwdPath, toPath(UNPACK_ZIP_PWD), true, { password: 'secret123' });
      log(unpackZipPwd.success ? '✅ Unpack encrypted zip OK' : `❌ Unpack FAILED: ${unpackZipPwd.errorMessage}`, unpackZipPwd.success);

      verifyRoundTrip('hello.txt (encrypted zip)', new File(SOURCE_DIR, 'hello.txt'), new File(UNPACK_ZIP_PWD, 'hello.txt'));

      // Unpack with wrong password
      const wrongPwd = await unpack(zipPwdPath, toPath(UNPACK_ZIP_PWD), true, { password: 'wrong' });
      if (!wrongPwd.success && wrongPwd.errorMessage.length > 0) {
        log(`✅ Wrong password rejected: ${wrongPwd.errorMessage}`);
      } else {
        log('❌ Should have failed with wrong password!', false);
      }

      // ===== 17. testArchive =====
      log('');
      log('━━━ 17. testArchive ━━━');
      const testZipOk = await testArchive(zipPath);
      log(testZipOk ? '✅ testArchive(zip) = true' : '❌ testArchive(zip) should be true', testZipOk);

      const testBz2Ok = await testArchive(bz2Path);
      log(testBz2Ok ? '✅ testArchive(tar.bz2) = true' : '❌ testArchive(tar.bz2) should be true', testBz2Ok);

      const testBad = await testArchive('/nonexistent.zip');
      log(!testBad ? '✅ testArchive(nonexistent) = false' : '❌ testArchive(nonexistent) should be false', !testBad);

      // ===== 18. Pack 7z → error =====
      log('');
      log('━━━ 18. Pack 7z → error ━━━');
      const pack7z = await pack(srcPath, toPath(new File(TEST_DIR, 'test.7z')), 'sevenz');
      if (!pack7z.success && pack7z.errorMessage.includes('not supported')) {
        log(`✅ Pack 7z correctly rejected: ${pack7z.errorMessage}`);
      } else {
        log('❌ Pack 7z should return "not supported" error!', false);
      }

      // ===== 19. 7z unpack =====
      log('');
      log('━━━ 19. 7z unpack ━━━');
      // Write base64 .7z to disk
      const archive7zPath = toPath(ARCHIVE_7Z);
      ARCHIVE_7Z.create();
      const binaryStr = atob(TEST_7Z_BASE64);
      const bytes = new Uint8Array(binaryStr.length);
      for (let idx = 0; idx < binaryStr.length; idx++) {
        bytes[idx] = binaryStr.charCodeAt(idx);
      }
      ARCHIVE_7Z.write(bytes);

      const detected7z = await detectFormat(archive7zPath);
      if (detected7z === 'sevenz') {
        log('✅ detectFormat = sevenz');
      } else {
        log(`❌ Expected sevenz, got: ${detected7z}`, false);
      }

      const contents7z = await listContents(archive7zPath);
      log(`📋 7z entries (${contents7z.length}):`);
      contents7z.forEach((e) =>
        log(`   • ${e.path} ${e.isDirectory ? '(dir)' : `(${e.size}B)`}`)
      );

      UNPACK_7Z.create();
      const unpack7z = await unpack(archive7zPath, toPath(UNPACK_7Z), true);
      log(unpack7z.success ? '✅ Unpack 7z OK' : `❌ Unpack 7z FAILED: ${unpack7z.errorMessage}`, unpack7z.success);

      // Verify 7z content
      const hello7z = new File(UNPACK_7Z, 'hello7z.txt');
      if (hello7z.exists && hello7z.textSync().includes('Hello from 7z!')) {
        log('✅ hello7z.txt content matches');
      } else {
        log('❌ hello7z.txt content mismatch!', false);
      }
      const polish7z = new File(UNPACK_7Z, 'polish7z.txt');
      if (polish7z.exists && polish7z.textSync().includes('jaźń')) {
        log('✅ polish7z.txt UTF-8 content matches');
      } else {
        log('❌ polish7z.txt UTF-8 content mismatch!', false);
      }

      // ===== 20. 7z testArchive =====
      log('');
      log('━━━ 20. 7z testArchive ━━━');
      const test7zOk = await testArchive(archive7zPath);
      log(test7zOk ? '✅ testArchive(7z) = true' : '❌ testArchive(7z) should be true', test7zOk);

      // ===== 21. ZIP with progress =====
      log('');
      log('━━━ 21. ZIP with progress ━━━');
      const zipProgressVals: number[] = [];
      const zipProgressArchive = new File(TEST_DIR, 'progress-test.zip');
      const packZipProgress = await pack(
        toPath(LARGE_DIR),
        toPath(zipProgressArchive),
        'zip',
        undefined,
        (bytesProcessed, totalBytes) => {
          const p = totalBytes > 0 ? bytesProcessed / totalBytes : 0;
          zipProgressVals.push(p);
        }
      );
      log(packZipProgress.success ? '✅ Pack zip with progress OK' : `❌ FAILED: ${packZipProgress.errorMessage}`, packZipProgress.success);
      if (zipProgressVals.length >= 2) {
        log(`✅ ZIP progress callbacks: ${zipProgressVals.length}`);
      } else {
        log(`❌ Too few ZIP progress callbacks: ${zipProgressVals.length}`, false);
      }

      // ===== SUMMARY =====
      log('');
      setProgressBar(1);
      log('🎉 All tests complete!');
    } catch (e: any) {
      log(`💥 Exception: ${e.message ?? String(e)}`, false);
    } finally {
      setRunning(false);
    }
  };

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar style="auto" />
      <Text style={styles.title}>react-native-nitro-archive</Text>
      <Text style={styles.subtitle}>Example & Test Runner</Text>

      {running && (
        <View style={styles.progressOuter}>
          <View
            style={[styles.progressInner, { width: `${progressBar * 100}%` }]}
          />
        </View>
      )}

      <Pressable
        style={[styles.button, running && styles.buttonDisabled]}
        onPress={runTests}
        disabled={running}
      >
        <Text style={styles.buttonText}>
          {running ? 'Running...' : 'Run Tests'}
        </Text>
      </Pressable>

      <ScrollView ref={scrollRef} style={styles.logContainer} contentContainerStyle={styles.scrollContainer}>
        {logs.map((entry, i) => (
          <Text
            key={i}
            style={[
              styles.logLine,
              !entry.ok && styles.logError,
              entry.text.startsWith('━') && styles.logSection,
            ]}
          >
            {entry.text}
          </Text>
        ))}
      </ScrollView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f5f5',
  },
  title: {
    fontSize: 24,
    fontWeight: 'bold',
    textAlign: 'center',
    marginTop: 20,
  },
  subtitle: {
    fontSize: 14,
    color: '#666',
    textAlign: 'center',
    marginBottom: 12,
  },
  progressOuter: {
    height: 6,
    backgroundColor: '#ddd',
    borderRadius: 3,
    marginHorizontal: 20,
    marginBottom: 12,
    overflow: 'hidden',
  },
  progressInner: {
    height: '100%',
    backgroundColor: '#34C759',
    borderRadius: 3,
  },
  button: {
    backgroundColor: '#007AFF',
    marginHorizontal: 20,
    paddingVertical: 14,
    borderRadius: 10,
    alignItems: 'center',
  },
  buttonDisabled: {
    backgroundColor: '#999',
  },
  buttonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '600',
  },
  logContainer: {
    flex: 1,
    margin: 16,
    marginHorizontal: 12,
    backgroundColor: '#1e1e1e',
    borderRadius: 10,
    padding: 12,
  },
  logLine: {
    color: '#d4d4d4',
    fontSize: 12,
    fontFamily: Platform.OS === 'ios' ? 'Menlo' : 'monospace',
    lineHeight: 18,
  },
  logError: {
    color: '#f44336',
  },
  logSection: {
    color: '#81D4FA',
    fontWeight: 'bold',
    marginTop: 4,
  },
  scrollContainer: {
    paddingBottom: 20,
  },
});
