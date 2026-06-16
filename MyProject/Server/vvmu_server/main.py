from fastapi import FastAPI
import mysql.connector
from datetime import datetime

app = FastAPI()

# Конфигурация на базата данни
db_config = {
    "host": "localhost",
    "user": "root",
    "password": "Cska-2003",
    "database": "vvmu_schedule"
}

def get_db():
    return mysql.connector.connect(**db_config)

def transliterate(text):
    mapping = {
        'а': 'a', 'б': 'b', 'в': 'v', 'г': 'g', 'д': 'd',
        'е': 'e', 'ж': 'zh', 'з': 'z', 'и': 'i', 'й': 'y',
        'к': 'k', 'л': 'l', 'м': 'm', 'н': 'n', 'о': 'o',
        'п': 'p', 'р': 'r', 'с': 's', 'т': 't', 'у': 'u',
        'ф': 'f', 'х': 'h', 'ц': 'ts', 'ч': 'ch', 'ш': 'sh',
        'щ': 'sht', 'ъ': 'a', 'ь': 'y', 'ю': 'yu', 'я': 'ya',
        'А': 'A', 'Б': 'B', 'В': 'V', 'Г': 'G', 'Д': 'D',
        'Е': 'E', 'Ж': 'Zh', 'З': 'Z', 'И': 'I', 'Й': 'Y',
        'К': 'K', 'Л': 'L', 'М': 'M', 'Н': 'N', 'О': 'O',
        'П': 'P', 'Р': 'R', 'С': 'S', 'Т': 'T', 'У': 'U',
        'Ф': 'F', 'Х': 'H', 'Ц': 'Ts', 'Ч': 'Ch', 'Ш': 'Sh',
        'Щ': 'Sht', 'Ъ': 'A', 'Ь': 'Y', 'Ю': 'Yu', 'Я': 'Ya'
    }
    return ''.join(mapping.get(c, c) for c in text)

def shorten_title(teacher):
    replacements = [
        ('к-н I р. проф. д-р', 'prof.'),
        ('гл. асистент д-р', 'gl.as.dr.'),
        ('гл. асистент', 'gl.as.'),
        ('лейт. асистент', 'lt.as.'),
        ('доц. д-р', 'doc.dr.'),
        ('доц.', 'doc.'),
        ('асистент', 'as.'),
        ('проф. д-р', 'prof.dr.'),
        ('проф.', 'prof.'),
    ]
    for full, short in replacements:
        if full in teacher:
            teacher = teacher.replace(full, short)
            break
    return teacher

def fetch_schedule(day_name: str):
    db = get_db()
    cursor = db.cursor(dictionary=True)
    
    query = """
        SELECT 
            s.id,
            s.day,
            s.start_time,
            s.end_time,
            h.number as hall,
            CONCAT(t.title, ' ', t.name) as teacher,
            d.name as discipline,
            d.type as type,
            cu.number as class_unit
        FROM Schedule s
        JOIN Halls h ON s.hall_id = h.id
        JOIN Teachers t ON s.teacher_id = t.id
        JOIN Disciplines d ON s.discipline_id = d.id
        JOIN Class_Units cu ON s.class_unit_id = cu.id
        WHERE s.day = %s
        ORDER BY s.start_time
    """
    
    cursor.execute(query, (day_name,))
    result = cursor.fetchall()
    
    for row in result:
        if row['start_time']:
            row['start_time'] = str(row['start_time'])
        if row['end_time']:
            row['end_time'] = str(row['end_time'])
        row['teacher'] = transliterate(shorten_title(row['teacher']))
        row['discipline'] = transliterate(row['discipline'])
        row['type'] = transliterate(row['type'])
    
    cursor.close()
    db.close()
    
    return {"day": day_name, "schedule": result}

DAYS = {
    0: "Неделя",
    1: "Понеделник",
    2: "Вторник",
    3: "Сряда",
    4: "Четвъртък",
    5: "Петък",
    6: "Събота"
}

@app.get("/")
def root():
    return {"message": "VVMU Schedule API работи!"}

# ВАЖНО: по-специфичният route трябва да е ПРЕДИ общия!
@app.get("/schedule/day/{day_number}")
def get_schedule_by_number(day_number: int):
    if day_number not in DAYS:
        return {"error": f"Invalid day number. Use 0-6 (0=Sunday, 1=Monday...)"}
    return fetch_schedule(DAYS[day_number])

@app.get("/schedule/{day}")
def get_schedule(day: str):
    return fetch_schedule(day)