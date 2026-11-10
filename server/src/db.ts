import Database from 'better-sqlite3';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const dbPath = path.resolve(__dirname, '../attendance.db');

export const db = new Database(dbPath);
db.pragma('journal_mode = WAL');

export function initDatabase() {
  db.exec(`
    CREATE TABLE IF NOT EXISTS students (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      uid TEXT UNIQUE NOT NULL,
      mssv TEXT UNIQUE NOT NULL,
      hoLot TEXT NOT NULL,
      ten TEXT NOT NULL,
      lop TEXT DEFAULT 'DI2396A1',
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );

    CREATE TABLE IF NOT EXISTS attendance_logs (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      student_id INTEGER,
      scanned_uid TEXT NOT NULL,
      status TEXT NOT NULL,
      timestamp DATETIME DEFAULT (datetime('now', 'localtime')),
      FOREIGN KEY (student_id) REFERENCES students(id) ON DELETE SET NULL
    );
  `);

  const count = db.prepare('SELECT COUNT(*) as count FROM students').get() as { count: number };
  if (count.count === 0) {
    const insert = db.prepare(`
      INSERT INTO students (uid, mssv, hoLot, ten, lop) 
      VALUES (?, ?, ?, ?, ?)
    `);

    insert.run('7340D637', 'B2303828', 'Nguyen Phuoc', 'Loc', 'DI2396A1');
    insert.run('7BA52C54', 'B2303829', 'Vo Thanh', 'Nam', 'DI2396A1');
    insert.run('E3A53038', 'B2303844', 'Tran Trong', 'Phuc', 'DI2396A1');
  }
}
