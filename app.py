import os
import psycopg2
from dotenv import load_dotenv
from flask import Flask, request
from datetime import datetime, timezone, timedelta

load_dotenv()

app = Flask(__name__)
url = os.environ.get("DATABASE_URL")
connection = psycopg2.connect(url)


#CONSTS
CREATE_AGENTS_TABLE = (
    "CREATE TABLE IF NOT EXISTS agents (id SERIAL PRIMARY KEY, ip CIDR, mac MACADDR, installTime TIMESTAMP);"
)
INSERT_AGENT_RETURN_ID = "INSERT INTO agents (ip, mac, installTime) VALUES (%s, %s, %s) RETURNING id;"



CREATE_BEACONS_TABLE = """CREATE TABLE IF NOT EXISTS beacons (agent_id INTEGER, time TIMESTAMP, FOREIGN KEY(agent_id) REFERENCES agents(id) ON DELETE CASCADE);"""
INSERT_BEACON = (
    "INSERT INTO beacons (agent_id, time) VALUES (%s, %s);"
)


LIST_AGENTS = """SELECT * FROM agents"""
LIST_BEACONS = """SELECT * FROM beacons"""

LIST_BEACON_BY_AGENT = """SELECT agents.id, agents.ip, agents.mac, agents.installTime, beacons.time FROM agents LEFT JOIN beacons ON agents.id = beacons.agent_id WHERE agents.id = (%s);"""


@app.post("/api/new_agent")
def create_agent():
    data = request.get_json()
    print(data)
    ip = data["ip"]
    mac = data["mac"]
    time = ""
    try:
        time = datetime.strptime(data["time"], "%m-%d-%Y %H:%M:%S")
    except KeyError:
        time = datetime.now(timezone.utc)
    with connection:
        with connection.cursor() as cursor:
            cursor.execute(CREATE_AGENTS_TABLE)
            cursor.execute(INSERT_AGENT_RETURN_ID, (ip, mac, time))
            agent_id = cursor.fetchone()[0]
        return {"id": agent_id, "message": f"Agent for {ip} -- {mac} created."}, 201
    

@app.post("/api/beacon")
def add_temp():
    data = request.get_json()
    agent_id = data["agent_id"]
    try:
        time = datetime.strptime(data["time"], "%m-%d-%Y %H:%M:%S")
    except KeyError:
        time = datetime.now(timezone.utc)
    with connection:
        with connection.cursor() as cursor:
            cursor.execute(CREATE_BEACONS_TABLE)
            cursor.execute(INSERT_BEACON, (agent_id, time))
    return {"message": "Beacon added."}, 201

@app.get("/api/list_agents")
def list_agents():
    with connection:
        with connection.cursor() as cursor:
            cursor.execute(LIST_AGENTS)
            agents = cursor.fetchall()
    return {"Agents": agents}

@app.get("/api/list_beacons")
def list_beacons():
    with connection:
        with connection.cursor() as cursor:
            cursor.execute(LIST_BEACONS)
            agents = cursor.fetchall()
    return {"beacons": agents}

@app.get("/api/agent/<int:agent_id>")
def agent_beacons(agent_id):
    with connection:
        with connection.cursor() as cursor:
            cursor.execute(LIST_BEACON_BY_AGENT, (agent_id,))
            beacons = cursor.fetchall()
    return {"beacons": beacons}