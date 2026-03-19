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
  packWithProgress,
  unpackWithProgress,
} from 'react-native-tar-bz2';
import { SafeAreaView } from 'react-native-safe-area-context';

/** Strip file:// prefix to get a plain filesystem path */
const toPath = (f: { uri: string }) => f.uri.replace(/^file:\/\//, '');

const TEST_DIR = new Directory(Paths.cache, 'tar-bz2-test');
const SOURCE_DIR = new Directory(TEST_DIR, 'source');
const SUBDIR = new Directory(SOURCE_DIR, 'subdir');
const EMPTY_DIR = new Directory(SOURCE_DIR, 'empty-dir');
const ARCHIVE_FILE = new File(TEST_DIR, 'test.tar.bz2');
const ARCHIVE_PROGRESS = new File(TEST_DIR, 'progress-test.tar.bz2');
const UNPACK_DIR = new Directory(TEST_DIR, 'unpacked');
const UNPACK_PROGRESS_DIR = new Directory(TEST_DIR, 'unpacked-progress');
const LARGE_DIR = new Directory(TEST_DIR, 'large-source');
const LARGE_ARCHIVE = new File(TEST_DIR, 'large.tar.bz2');
const LARGE_UNPACK_DIR = new Directory(TEST_DIR, 'large-unpacked');

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
    hello.write('Hello, tar.bz2!');

    const polish = new File(SOURCE_DIR, 'polskie-znaki.txt');
    polish.create();
    polish.write('Zażółć gęślą jaźń — łódź');

    const nested = new File(SUBDIR, 'nested.txt');
    nested.create();
    nested.write('Nested file content');
  };

  /** Generate ~1MB of files for progress callback testing */
  const setupLargeFiles = () => {
    LARGE_DIR.create();
    const chunk = 'abcdefghij'.repeat(100); // 1KB per file × 10 files
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

  const runTests = async () => {
    setRunning(true);
    setLogs([]);
    setProgressBar(0);

    const srcPath = toPath(SOURCE_DIR);
    const archivePath = toPath(ARCHIVE_FILE);
    const unpackPath = toPath(UNPACK_DIR);

    try {
      // ===== SETUP =====
      cleanup();
      TEST_DIR.create();
      log('🧹 Cleanup done');

      setupTestFiles();
      log('📁 Test files created');

      // ===== 1. PACK =====
      log('');
      log('━━━ 1. Pack ━━━');
      const packResult = await pack(srcPath, archivePath);
      if (packResult.success) {
        log(`✅ Pack OK`);
      } else {
        log(`❌ Pack FAILED: ${packResult.error}`, false);
        return;
      }

      if (ARCHIVE_FILE.exists) {
        log('✅ Archive file created');
      } else {
        log('❌ Archive file not found!', false);
        return;
      }

      // ===== 2. LIST CONTENTS =====
      log('');
      log('━━━ 2. List Contents ━━━');
      const contents = await listContents(archivePath);
      log(`📋 Entries (${contents.length}):`);
      contents.forEach((e) => log(`   • ${e}`));
      if (contents.length === 5) {
        log('✅ Entry count correct');
      } else {
        log(`❌ Expected 5 entries, got ${contents.length}`, false);
      }

      // ===== 3. UNPACK =====
      log('');
      log('━━━ 3. Unpack ━━━');
      UNPACK_DIR.create();
      const unpackResult = await unpack(archivePath, unpackPath, true);
      if (unpackResult.success) {
        log('✅ Unpack OK');
      } else {
        log(`❌ Unpack FAILED: ${unpackResult.error}`, false);
        return;
      }

      // ===== 4. ROUND-TRIP VERIFICATION =====
      log('');
      log('━━━ 4. Round-trip verification ━━━');

      const checks: [string, File, File][] = [
        [
          'hello.txt',
          new File(SOURCE_DIR, 'hello.txt'),
          new File(UNPACK_DIR, 'hello.txt'),
        ],
        [
          'polskie-znaki.txt',
          new File(SOURCE_DIR, 'polskie-znaki.txt'),
          new File(UNPACK_DIR, 'polskie-znaki.txt'),
        ],
        [
          'subdir/nested.txt',
          new File(SUBDIR, 'nested.txt'),
          new File(new Directory(UNPACK_DIR, 'subdir'), 'nested.txt'),
        ],
      ];

      for (const [name, orig, unpacked] of checks) {
        const a = orig.textSync();
        const b = unpacked.textSync();
        if (a === b) {
          log(`✅ ${name} matches`);
        } else {
          log(`❌ ${name} mismatch!`, false);
        }
      }

      // ===== 5. OVERWRITE=FALSE =====
      log('');
      log('━━━ 5. Overwrite flag ━━━');
      const modFile = new File(UNPACK_DIR, 'hello.txt');
      modFile.write('MODIFIED');
      await unpack(archivePath, unpackPath, false);
      if (modFile.textSync() === 'MODIFIED') {
        log('✅ overwrite=false preserved existing file');
      } else {
        log('❌ overwrite=false did NOT preserve file!', false);
      }

      // ===== 6. ERROR HANDLING =====
      log('');
      log('━━━ 6. Error handling ━━━');

      const badUnpack = await unpack('/nonexistent.tar.bz2', unpackPath, true);
      if (!badUnpack.success && badUnpack.error.length > 0) {
        log(`✅ Bad unpack: ${badUnpack.error}`);
      } else {
        log('❌ Should have failed on nonexistent file!', false);
      }

      const badPack = await pack('/nonexistent-dir', archivePath);
      if (!badPack.success && badPack.error.length > 0) {
        log(`✅ Bad pack: ${badPack.error}`);
      } else {
        log('❌ Should have failed on nonexistent source!', false);
      }

      // ===== 7. PACK WITH PROGRESS =====
      log('');
      log('━━━ 7. Pack with progress callback ━━━');
      setupLargeFiles();

      const progressValsPack: number[] = [];
      setProgressBar(0);
      const packProgressResult = await packWithProgress(
        toPath(LARGE_DIR),
        toPath(LARGE_ARCHIVE),
        (p) => {
          progressValsPack.push(p);
          setProgressBar(p);
        }
      );

      if (packProgressResult.success) {
        log(`✅ packWithProgress OK`);
      } else {
        log(`❌ packWithProgress FAILED: ${packProgressResult.error}`, false);
      }

      if (progressValsPack.length >= 2) {
        log(`✅ Pack progress callbacks: ${progressValsPack.length}`);
        log(`   first=${progressValsPack[0]!.toFixed(2)}, last=${progressValsPack[progressValsPack.length - 1]!.toFixed(2)}`);
      } else {
        log(`❌ Too few pack progress callbacks: ${progressValsPack.length}`, false);
      }

      const isMonotonic = progressValsPack.every(
        (v, i) => i === 0 || v >= progressValsPack[i - 1]!
      );
      if (isMonotonic) {
        log('✅ Pack progress is monotonically increasing');
      } else {
        log('❌ Pack progress is NOT monotonic!', false);
      }

      if (
        progressValsPack[0]! <= 0.01 &&
        progressValsPack[progressValsPack.length - 1]! >= 0.99
      ) {
        log('✅ Pack progress range: 0.0 → 1.0');
      } else {
        log('❌ Pack progress range incorrect', false);
      }

      // ===== 8. UNPACK WITH PROGRESS =====
      log('');
      log('━━━ 8. Unpack with progress callback ━━━');
      LARGE_UNPACK_DIR.create();

      const progressValsUnpack: number[] = [];
      setProgressBar(0);
      const unpackProgressResult = await unpackWithProgress(
        toPath(LARGE_ARCHIVE),
        toPath(LARGE_UNPACK_DIR),
        true,
        (p) => {
          progressValsUnpack.push(p);
          setProgressBar(p);
        }
      );

      if (unpackProgressResult.success) {
        log('✅ unpackWithProgress OK');
      } else {
        log(`❌ unpackWithProgress FAILED: ${unpackProgressResult.error}`, false);
      }

      if (progressValsUnpack.length >= 2) {
        log(`✅ Unpack progress callbacks: ${progressValsUnpack.length}`);
        log(`   first=${progressValsUnpack[0]!.toFixed(2)}, last=${progressValsUnpack[progressValsUnpack.length - 1]!.toFixed(2)}`);
      } else {
        log(`❌ Too few unpack progress callbacks: ${progressValsUnpack.length}`, false);
      }

      const isMonotonicU = progressValsUnpack.every(
        (v, i) => i === 0 || v >= progressValsUnpack[i - 1]!
      );
      if (isMonotonicU) {
        log('✅ Unpack progress is monotonically increasing');
      } else {
        log('❌ Unpack progress is NOT monotonic!', false);
      }

      // Verify large round-trip (spot-check 3 random files)
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
      <Text style={styles.title}>react-native-tar-bz2</Text>
      <Text style={styles.subtitle}>Example & Test Runner</Text>

      {/* Progress bar */}
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
    paddingTop: Platform.OS === 'android' ? 40 : 0,
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
    paddingBottom: 20
  }
});
