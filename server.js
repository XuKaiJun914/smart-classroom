const express = require('express');
const WebSocket = require('ws');
const mysql = require('mysql2/promise');
const path = require('path');
const bcrypt = require('bcrypt');
const http = require('http');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

const dbConfig = {
    host: 'localhost',
    user: 'root',
    password: '',
    database: 'smart_classroom'
};

app.use(express.static(path.join(__dirname, 'public')));

async function getDb() {
    return await mysql.createConnection(dbConfig);
}

function getSafeRoomName(name, id) {
    // eslint-disable-next-line no-control-regex
    if (/[^\x00-\x7F]/.test(name)) {
        if (id == 1) return "IoT Lab";
        if (id == 2) return "Media Room";
        return "Room " + id;
    }
    return name;
}

wss.on('connection', (ws) => {
    ws.isAlive = true;
    ws.on('pong', () => ws.isAlive = true);

    ws.on('message', async (message) => {
        try {
            const msg = JSON.parse(message);
            await handleMessage(ws, msg);
        } catch (e) {
            console.error(e);
        }
    });
});

const interval = setInterval(() => {
    wss.clients.forEach((ws) => {
        if (ws.isAlive === false) return ws.terminate();
        ws.isAlive = false;
        ws.ping();
    });
}, 30000);

wss.on('close', () => clearInterval(interval));

async function handleMessage(ws, msg) {
    const db = await getDb();
    try {
        switch (msg.action) {
            case 'login':
                const [users] = await db.execute('SELECT * FROM admin_users WHERE username = ?', [msg.username]);
                if (users.length > 0 && await bcrypt.compare(msg.password, users[0].password)) {
                    ws.isAuthenticated = true;
                    ws.send(JSON.stringify({ type: 'login_response', status: 'success' }));
                } else {
                    ws.send(JSON.stringify({ type: 'login_response', status: 'error', message: 'Invalid credentials' }));
                }
                break;

            case 'check_access':
                const uid = msg.uid;
                const cid = msg.cid || 1;
                
                const [rooms] = await db.execute('SELECT * FROM classroom_status WHERE id = ?', [cid]);
                if(rooms.length === 0) return;
                const room = rooms[0];
                let rName = getSafeRoomName(room.name, cid);

                if(room.is_locked == 1) {
                    ws.send(JSON.stringify({ type: 'access_result', status: 'LOCKED', message: 'Room Locked', room_name: rName }));
                    await logAction(db, cid, uid, 'LOCKED', 'Unknown');
                } else {
                    const [stus] = await db.execute('SELECT * FROM students WHERE uid = ? AND can_enter = 1', [uid]);
                    if(stus.length > 0) {
                        const [logs] = await db.execute("SELECT id FROM logs WHERE classroom_id = ? AND student_uid = ? AND action = 'ACCESS_GRANTED' AND timestamp > (NOW() - INTERVAL 1 MINUTE)", [cid, uid]);
                        
                        if(logs.length === 0) {
                            await db.execute('UPDATE classroom_status SET current_count = current_count + 1 WHERE id = ?', [cid]);
                            await logAction(db, cid, uid, 'ACCESS_GRANTED', stus[0].name);
                        }
                        
                        ws.send(JSON.stringify({ type: 'access_result', status: 'GRANTED', message: 'Welcome', room_name: rName }));
                        broadcastDashboard(cid); 
                    } else {
                        ws.send(JSON.stringify({ type: 'access_result', status: 'DENIED', message: 'Access Denied', room_name: rName }));
                        await logAction(db, cid, uid, 'ACCESS_DENIED', 'Unknown');
                    }
                }
                break;

            case 'env_update':
                await db.execute('UPDATE classroom_status SET temperature = ?, humidity = ? WHERE id = ?', [msg.temp, msg.hum, msg.cid]);
                broadcastDashboard(msg.cid);
                break;

            case 'toggle_lock':
                await db.execute('UPDATE classroom_status SET is_locked = NOT is_locked WHERE id = ?', [msg.cid]);
                broadcastDashboard(msg.cid);
                break;

            case 'get_dashboard':
                if (!ws.isAuthenticated) return;
                const [dRooms] = await db.execute('SELECT * FROM classroom_status WHERE id = ?', [msg.cid]);
                if(dRooms.length > 0) {
                    let dData = dRooms[0];
                    dData.name = getSafeRoomName(dData.name, msg.cid);
                    ws.send(JSON.stringify({ type: 'dashboard_data', data: dData }));
                }
                break;

            case 'get_students':
                if (!ws.isAuthenticated) return;
                const [sList] = await db.execute('SELECT * FROM students ORDER BY id DESC');
                ws.send(JSON.stringify({ type: 'student_list', data: sList }));
                break;

            case 'save_student':
                if (!ws.isAuthenticated) return;
                const [ex] = await db.execute('SELECT id FROM students WHERE uid = ?', [msg.uid]);
                if(ex.length > 0) {
                    await db.execute('UPDATE students SET name=?, class_name=? WHERE uid=?', [msg.name, msg.class_name, msg.uid]);
                } else {
                    await db.execute('INSERT INTO students (uid, name, class_name) VALUES (?, ?, ?)', [msg.uid, msg.name, msg.class_name]);
                }
                ws.send(JSON.stringify({ type: 'operation_success', action: 'save_student' }));
                break;

            case 'delete_student':
                if (!ws.isAuthenticated) return;
                await db.execute('DELETE FROM students WHERE uid = ?', [msg.uid]);
                ws.send(JSON.stringify({ type: 'operation_success', action: 'delete_student' }));
                break;

            case 'get_classrooms':
                const [cList] = await db.execute('SELECT * FROM classroom_status');
                const safeList = cList.map(c => ({...c, name: getSafeRoomName(c.name, c.id)}));
                ws.send(JSON.stringify({ type: 'classroom_list', data: safeList }));
                break;

            case 'save_classroom':
                if (!ws.isAuthenticated) return;
                if(msg.id > 0) {
                    await db.execute('UPDATE classroom_status SET name=?, capacity=? WHERE id=?', [msg.name, msg.capacity, msg.id]);
                } else {
                    await db.execute('INSERT INTO classroom_status (name, capacity, current_count, is_locked) VALUES (?, ?, 0, 1)', [msg.name, msg.capacity]);
                }
                ws.send(JSON.stringify({ type: 'operation_success', action: 'save_classroom' }));
                break;
            
            case 'delete_classroom':
                if (!ws.isAuthenticated) return;
                if(msg.id != 1) {
                    await db.execute('DELETE FROM classroom_status WHERE id = ?', [msg.id]);
                    ws.send(JSON.stringify({ type: 'operation_success', action: 'delete_classroom' }));
                }
                break;

            case 'get_logs':
                if (!ws.isAuthenticated) return;
                const [lList] = await db.execute('SELECT * FROM logs WHERE classroom_id = ? ORDER BY timestamp DESC LIMIT 50', [msg.cid]);
                ws.send(JSON.stringify({ type: 'log_list', data: lList }));
                break;

            case 'reset_count':
                if (!ws.isAuthenticated) return;
                await db.execute('UPDATE classroom_status SET current_count = 0 WHERE id = ?', [msg.cid]);
                broadcastDashboard(msg.cid);
                break;
            
            case 'update_count':
                if (!ws.isAuthenticated) return;
                await db.execute('UPDATE classroom_status SET current_count = ? WHERE id = ?', [msg.count, msg.cid]);
                broadcastDashboard(msg.cid);
                break;
        }
    } catch(err) {
        console.error(err);
    } finally {
        await db.end();
    }
}

async function logAction(db, cid, uid, action, name) {
    await db.execute('INSERT INTO logs (classroom_id, student_uid, action, student_name) VALUES (?, ?, ?, ?)', [cid, uid, action, name]);
    broadcastLogs(cid);
}

async function broadcastDashboard(cid) {
    const db = await getDb();
    const [rows] = await db.execute('SELECT * FROM classroom_status WHERE id = ?', [cid]);
    await db.end();
    
    if(rows.length > 0) {
        let rData = rows[0];
        rData.name = getSafeRoomName(rData.name, cid);
        const msg = JSON.stringify({ type: 'dashboard_update', data: rData, cid: cid });
        wss.clients.forEach(client => {
            if (client.readyState === WebSocket.OPEN) client.send(msg);
        });
    }
}

async function broadcastLogs(cid) {
    const db = await getDb();
    const [rows] = await db.execute('SELECT * FROM logs WHERE classroom_id = ? ORDER BY timestamp DESC LIMIT 50', [cid]);
    await db.end();

    const msg = JSON.stringify({ type: 'log_update', data: rows, cid: cid });
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN && client.isAuthenticated) client.send(msg);
    });
}

server.listen(8080, async () => {
    console.log('Server started on port 8080');
    try {
        const db = await getDb();
        const hashedPassword = await bcrypt.hash('admin123', 10);
        await db.execute("DELETE FROM admin_users WHERE username = 'admin'");
        await db.execute("INSERT INTO admin_users (username, password) VALUES ('admin', ?)", [hashedPassword]);
        console.log("Admin account reset to: admin / admin123");
        await db.end();
    } catch (e) {
        console.error("Failed to reset admin password:", e);
    }
});