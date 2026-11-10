import { Router, Request, Response } from 'express';
import ExcelJS from 'exceljs';
import { db } from '../db.js';
import { addSSEClient, removeSSEClient, broadcastEvent } from '../sse.js';

export const apiRouter = Router();

let isEnrollMode = false;
let latestEnrollScan: { uid: string; timestamp: number } | null = null;

function formatShortName(hoLot: string, ten: string): string {
  const parts = hoLot.trim().split(/\s+/).filter(Boolean);
  const initials = parts.map(p => p.charAt(0).toUpperCase() + '.').join('');
  return `${initials}${ten.trim()}`;
}

apiRouter.get('/events', (req: Request, res: Response) => {
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache, no-transform');
  res.setHeader('Connection', 'keep-alive');
  res.setHeader('X-Accel-Buffering', 'no');
  res.flushHeaders();

  const clientId = addSSEClient(res);
  res.write(`event: connected\ndata: ${JSON.stringify({ clientId, enrollMode: isEnrollMode })}\n\n`);

  req.on('close', () => {
    removeSSEClient(clientId);
  });
});

apiRouter.get('/system/mode', (_req: Request, res: Response) => {
  res.json({ enrollMode: isEnrollMode });
});

apiRouter.post('/system/mode', (req: Request, res: Response) => {
  isEnrollMode = Boolean(req.body.enrollMode);
  latestEnrollScan = null;
  broadcastEvent('mode_changed', { enrollMode: isEnrollMode });
  res.json({ success: true, enrollMode: isEnrollMode });
});

apiRouter.get('/system/enroll-scan', (_req: Request, res: Response) => {
  res.json({ scan: latestEnrollScan });
});

apiRouter.post('/system/enroll-scan/clear', (_req: Request, res: Response) => {
  latestEnrollScan = null;
  res.json({ success: true });
});

apiRouter.get('/attendance/latest', (req: Request, res: Response) => {
  const sinceId = Number(req.query.since || 0);
  const rows = db.prepare(`
    SELECT l.id, l.timestamp, l.status, l.scanned_uid,
           s.mssv, s.hoLot, s.ten, s.lop
    FROM attendance_logs l
    LEFT JOIN students s ON l.student_id = s.id
    WHERE l.id > ?
    ORDER BY l.id ASC
  `).all(sinceId);
  res.json(rows);
});

apiRouter.post('/attendance/scan', (req: Request, res: Response) => {
  let { uid } = req.body;
  if (!uid || typeof uid !== 'string') {
    return res.status(400).json({ error: 'UID is required' });
  }

  uid = uid.replace(/\s+/g, '').toUpperCase();

  if (isEnrollMode) {
    latestEnrollScan = { uid, timestamp: Date.now() };
    const existing = db.prepare('SELECT * FROM students WHERE uid = ?').get(uid);
    broadcastEvent('card_scanned_enroll', { uid, existingStudent: existing || null });
    return res.json({
      status: 'ENROLL_MODE',
      uid,
      message: 'The da quet vao he thong ghi danh'
    });
  }

  const student = db.prepare('SELECT * FROM students WHERE uid = ?').get(uid) as any;

  if (student) {
    const stmt = db.prepare(`
      INSERT INTO attendance_logs (student_id, scanned_uid, status)
      VALUES (?, ?, 'SUCCESS')
    `);
    const result = stmt.run(student.id, uid);

    const logEntry = db.prepare(`
      SELECT l.id, l.timestamp, l.status, l.scanned_uid,
             s.mssv, s.hoLot, s.ten, s.lop
      FROM attendance_logs l
      LEFT JOIN students s ON l.student_id = s.id
      WHERE l.id = ?
    `).get(result.lastInsertRowid);

    broadcastEvent('attendance_updated', logEntry);

    const shortName = formatShortName(student.hoLot, student.ten);
    return res.json({
      status: 'SUCCESS',
      mssv: student.mssv,
      name: shortName,
      fullName: `${student.hoLot} ${student.ten}`,
      lop: student.lop
    });
  } else {
    const stmt = db.prepare(`
      INSERT INTO attendance_logs (student_id, scanned_uid, status)
      VALUES (NULL, ?, 'UNREGISTERED')
    `);
    const result = stmt.run(uid);

    const logEntry = db.prepare(`
      SELECT l.id, l.timestamp, l.status, l.scanned_uid,
             NULL as mssv, 'Chua dang ky' as hoLot, '' as ten, '' as lop
      FROM attendance_logs l
      WHERE l.id = ?
    `).get(result.lastInsertRowid);

    broadcastEvent('attendance_updated', logEntry);

    return res.json({
      status: 'NOT_REGISTERED',
      uid,
      message: 'The chua dang ky'
    });
  }
});

apiRouter.get('/students', (_req: Request, res: Response) => {
  const students = db.prepare('SELECT * FROM students ORDER BY ten ASC, hoLot ASC').all();
  res.json(students);
});

apiRouter.post('/students', (req: Request, res: Response) => {
  const { uid, mssv, hoLot, ten, lop } = req.body;
  if (!uid || !mssv || !hoLot || !ten) {
    return res.status(400).json({ error: 'Missing required fields' });
  }

  const cleanUid = uid.replace(/\s+/g, '').toUpperCase();
  const cleanMssv = mssv.trim().toUpperCase();

  try {
    const stmt = db.prepare(`
      INSERT INTO students (uid, mssv, hoLot, ten, lop)
      VALUES (?, ?, ?, ?, ?)
    `);
    const result = stmt.run(cleanUid, cleanMssv, hoLot.trim(), ten.trim(), (lop || 'DI2396A1').trim());
    const newStudent = db.prepare('SELECT * FROM students WHERE id = ?').get(result.lastInsertRowid);
    broadcastEvent('student_created', newStudent);
    res.status(201).json(newStudent);
  } catch (err: any) {
    res.status(400).json({ error: err.message || 'Error inserting student' });
  }
});

apiRouter.put('/students/:id', (req: Request, res: Response) => {
  const { id } = req.params;
  const { uid, mssv, hoLot, ten, lop } = req.body;

  try {
    const cleanUid = uid ? uid.replace(/\s+/g, '').toUpperCase() : undefined;
    const cleanMssv = mssv ? mssv.trim().toUpperCase() : undefined;

    const stmt = db.prepare(`
      UPDATE students
      SET uid = COALESCE(?, uid),
          mssv = COALESCE(?, mssv),
          hoLot = COALESCE(?, hoLot),
          ten = COALESCE(?, ten),
          lop = COALESCE(?, lop)
      WHERE id = ?
    `);
    stmt.run(cleanUid, cleanMssv, hoLot?.trim(), ten?.trim(), lop?.trim(), id);

    const updated = db.prepare('SELECT * FROM students WHERE id = ?').get(id);
    broadcastEvent('student_updated', updated);
    res.json(updated);
  } catch (err: any) {
    res.status(400).json({ error: err.message || 'Error updating student' });
  }
});

apiRouter.delete('/students/:id', (req: Request, res: Response) => {
  const { id } = req.params;
  db.prepare('DELETE FROM students WHERE id = ?').run(id);
  broadcastEvent('student_deleted', { id: Number(id) });
  res.json({ success: true });
});

apiRouter.get('/attendance/logs', (req: Request, res: Response) => {
  const { date } = req.query;
  let query = `
    SELECT l.id, l.timestamp, l.status, l.scanned_uid,
           s.mssv, s.hoLot, s.ten, s.lop
    FROM attendance_logs l
    LEFT JOIN students s ON l.student_id = s.id
  `;
  const params: any[] = [];

  if (date) {
    query += ` WHERE DATE(l.timestamp) = DATE(?)`;
    params.push(date);
  }

  query += ` ORDER BY l.id DESC LIMIT 200`;
  const logs = db.prepare(query).all(...params);
  res.json(logs);
});

apiRouter.get('/attendance/stats', (_req: Request, res: Response) => {
  const totalStudents = (db.prepare('SELECT COUNT(*) as count FROM students').get() as any).count;
  const todayChecked = (db.prepare(`
    SELECT COUNT(DISTINCT student_id) as count 
    FROM attendance_logs 
    WHERE status = 'SUCCESS' AND DATE(timestamp) = DATE('now', 'localtime')
  `).get() as any).count;
  const totalLogs = (db.prepare(`
    SELECT COUNT(*) as count 
    FROM attendance_logs 
    WHERE DATE(timestamp) = DATE('now', 'localtime')
  `).get() as any).count;

  res.json({
    totalStudents,
    todayChecked,
    todayAbsent: Math.max(0, totalStudents - todayChecked),
    totalLogs
  });
});

apiRouter.get('/attendance/export', async (req: Request, res: Response) => {
  const { date } = req.query;
  let query = `
    SELECT l.id, l.timestamp, l.status, l.scanned_uid,
           COALESCE(s.mssv, 'N/A') as mssv,
           COALESCE(s.hoLot, 'Chưa đăng ký') as hoLot,
           COALESCE(s.ten, '') as ten,
           COALESCE(s.lop, '') as lop
    FROM attendance_logs l
    LEFT JOIN students s ON l.student_id = s.id
  `;
  const params: any[] = [];
  if (date) {
    query += ` WHERE DATE(l.timestamp) = DATE(?)`;
    params.push(date);
  }
  query += ` ORDER BY l.id ASC`;

  const rows = db.prepare(query).all(...params) as any[];

  const workbook = new ExcelJS.Workbook();
  const worksheet = workbook.addWorksheet('Diem_Danh');

  worksheet.columns = [
    { header: 'STT', key: 'stt', width: 8 },
    { header: 'Mã Số SV', key: 'mssv', width: 15 },
    { header: 'Họ và Tên lót', key: 'hoLot', width: 25 },
    { header: 'Tên', key: 'ten', width: 12 },
    { header: 'Lớp', key: 'lop', width: 15 },
    { header: 'Mã UID Thẻ NFC', key: 'uid', width: 18 },
    { header: 'Thời Gian Quét', key: 'timestamp', width: 22 },
    { header: 'Trạng Thái', key: 'status', width: 18 }
  ];

  worksheet.getRow(1).font = { bold: true, color: { argb: 'FFFFFFFF' } };
  worksheet.getRow(1).fill = {
    type: 'pattern',
    pattern: 'solid',
    fgColor: { argb: 'FF58CC02' }
  };

  rows.forEach((r, idx) => {
    worksheet.addRow({
      stt: idx + 1,
      mssv: r.mssv,
      hoLot: r.hoLot,
      ten: r.ten,
      lop: r.lop,
      uid: r.scanned_uid,
      timestamp: r.timestamp,
      status: r.status === 'SUCCESS' ? 'Hợp lệ' : 'Chưa đăng ký'
    });
  });

  res.setHeader('Content-Type', 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet');
  res.setHeader('Content-Disposition', `attachment; filename="Bao_Cao_Diem_Danh_${date || 'all'}.xlsx"`);

  await workbook.xlsx.write(res);
  res.end();
});
