CREATE DATABASE IF NOT EXISTS smart_classroom DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE smart_classroom;

CREATE TABLE IF NOT EXISTS classroom_status (
  id int(11) NOT NULL AUTO_INCREMENT,
  name varchar(50) NOT NULL DEFAULT 'General Room',
  is_locked tinyint(1) DEFAULT 0,
  current_count int(11) DEFAULT 0,
  capacity int(11) DEFAULT 40, 
  temperature float DEFAULT 0,
  humidity float DEFAULT 0,
  updated_at timestamp DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id)
);

INSERT INTO classroom_status (id, name, is_locked, current_count, capacity) VALUES 
(1, '物聯網實驗室', 1, 0, 40),
(2, '多媒體教室', 0, 0, 50)
ON DUPLICATE KEY UPDATE id=id;

CREATE TABLE IF NOT EXISTS students (
  id int(11) NOT NULL AUTO_INCREMENT,
  uid varchar(20) NOT NULL,
  name varchar(50) NOT NULL,
  class_name varchar(50) DEFAULT 'Unclassified',
  can_enter tinyint(1) DEFAULT 1,
  created_at datetime DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uid (uid)
);

CREATE TABLE IF NOT EXISTS logs (
  id int(11) NOT NULL AUTO_INCREMENT,
  classroom_id int(11) NOT NULL DEFAULT 1,
  student_uid varchar(20) NOT NULL,
  student_name varchar(50) DEFAULT 'Unknown',
  action varchar(20) NOT NULL,
  timestamp timestamp DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id)
);

CREATE TABLE IF NOT EXISTS admin_users (
  id int(11) NOT NULL AUTO_INCREMENT,
  username varchar(50) NOT NULL,
  password varchar(255) NOT NULL,
  PRIMARY KEY (id),
  UNIQUE KEY username (username)
);

INSERT INTO admin_users (username, password) VALUES ('admin', '$2b$10$MDo/V.JErw8GGLcSxmLBLeqnO33NjpRKNlaTq3p2L/YlXlKXMr47e') ON DUPLICATE KEY;