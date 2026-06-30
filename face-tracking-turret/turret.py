"""
Face-Tracking Turret — Arduino Nano + StandardFirmata
======================================================
BEST VERSION — Full-featured with:
  - OpenCV face detection with adaptive smoothing
  - Zone-based X-axis servo control (7 zones with hysteresis)
  - Continuous Y-axis tracking
  - Pendulum-mode trigger servo (D12)
  - PWM buzzer with urgency-scaled beeping
  - Laser module (D4) — ON when face detected
  - Aim dwell timer with stability check before firing

Hardware:
  Servos: D9=X (zone-based), D11=Y (continuous), D12=trigger (pendulum)
  Buzzer: D3 (PWM), Laser: D4 (digital)

Dependencies: opencv-python, numpy, pyfirmata
Upload StandardFirmata to your Arduino Nano first.
"""

import inspect, time, threading, cv2, numpy as np
if not hasattr(inspect, "getargspec"):
    inspect.getargspec = inspect.getfullargspec
import pyfirmata

# ---------- CONFIG ----------
PORT      = "/dev/cu.usbserial-130"  # Change to your Arduino's port
X_PIN, Y_PIN, TRIG_PIN = 9, 11, 12
BUZZ_PIN, LASER_PIN = 3, 4
WS, HS = 640, 480
DETECT_SCALE = 0.5
INVERT_X, INVERT_Y = True, True
X_MIN, X_MAX = 45, 135
Y_MIN, Y_MAX = 30, 150
HOME_X, HOME_Y = 90, 90
X_ZONE_COUNT = 7
X_ZONE_HYSTERESIS_PX = 25
X_COMMAND_MIN_INTERVAL_SEC = 0.10
FACE_SMOOTH_X_BASE, FACE_SMOOTH_X_FAST = 0.30, 0.85
FACE_SMOOTH_Y_BASE, FACE_SMOOTH_Y_FAST = 0.35, 0.80
JUMP_THRESHOLD_PX, MAX_JUMP_PX = 55, 320
Y_GAIN = 1.0
DEADBAND_DEG = 1
AIM_DWELL_SEC = 1.2
AIM_STABLE_DEG = 6
AIM_STABLE_WINDOW = 0.35
REARM_LOST_SEC = 0.6
TRIGGER_STEP_DEG = 45
TRIGGER_STEP_TIME_SEC = 0.40
TRIGGER_MIN, TRIGGER_MAX = 0, 180
TRIG_START_ANGLE, TRIG_START_DIRECTION = 0, +1
BUZZ_DUTY_LOW, BUZZ_DUTY_HIGH = 0.25, 0.55
BUZZ_ON_SEC = 0.08
BUZZ_INTERVAL_SLOW, BUZZ_INTERVAL_FAST = 0.55, 0.13

# ---------- INIT ----------
print(f"Connecting to Arduino on {PORT}...")
board = pyfirmata.Arduino(PORT)
pyfirmata.util.Iterator(board).start()
time.sleep(4)
print("Firmata handshake done.")

servoX = board.get_pin(f"d:{X_PIN}:s")
servoY = board.get_pin(f"d:{Y_PIN}:s")
servoTrig = board.get_pin(f"d:{TRIG_PIN}:s")
buzzer = board.get_pin(f"d:{BUZZ_PIN}:p")
laser = board.get_pin(f"d:{LASER_PIN}:o")
buzzer.write(0); laser.write(0)
servoX.write(HOME_X); servoY.write(HOME_Y)
time.sleep(1.5)
print("Turret at home.")

# ---------- HELPERS ----------
laser_on = False
def set_laser(on):
    global laser_on
    if on == laser_on: return
    try: laser.write(1 if on else 0); laser_on = on
    except Exception as e: print(f"Laser write failed: {e}")

trigger_busy = False
trig_current_angle = TRIG_START_ANGLE
trig_direction = TRIG_START_DIRECTION
fire_count = 0

def smooth_move_trig(start, end, duration):
    if duration <= 0:
        try: servoTrig.write(int(end))
        except: pass
        return
    steps = max(8, int(abs(end - start) / 3))
    for i in range(steps + 1):
        a = start + (end - start) * i / steps
        try: servoTrig.write(int(a))
        except Exception as e: print(f"D12 write failed: {e}"); return
        time.sleep(duration / steps)

def fire_trigger():
    global trigger_busy, trig_current_angle, trig_direction, fire_count
    trigger_busy = True
    try:
        start = trig_current_angle
        target = start + trig_direction * TRIGGER_STEP_DEG
        if target > TRIGGER_MAX: target = TRIGGER_MAX; trig_direction = -1
        elif target < TRIGGER_MIN: target = TRIGGER_MIN; trig_direction = +1
        if target != start:
            distance = abs(target - start)
            smooth_move_trig(start, target, TRIGGER_STEP_TIME_SEC * distance / TRIGGER_STEP_DEG)
            trig_current_angle = target
        else:
            trig_direction = -trig_direction
            target = max(TRIGGER_MIN, min(TRIGGER_MAX, trig_current_angle + trig_direction * TRIGGER_STEP_DEG))
            if target != trig_current_angle:
                smooth_move_trig(trig_current_angle, target, TRIGGER_STEP_TIME_SEC)
                trig_current_angle = target
        if trig_current_angle >= TRIGGER_MAX: trig_direction = -1
        elif trig_current_angle <= TRIGGER_MIN: trig_direction = +1
        fire_count += 1
        print(f"D12 fire #{fire_count} -> {trig_current_angle} deg")
    finally:
        trigger_busy = False

buzzer_on = False; next_beep_at = 0.0; buzz_off_at = 0.0

def update_buzzer(active, urgency, now):
    global buzzer_on, next_beep_at, buzz_off_at
    if active:
        interval = BUZZ_INTERVAL_SLOW + (BUZZ_INTERVAL_FAST - BUZZ_INTERVAL_SLOW) * urgency
        duty = BUZZ_DUTY_LOW + (BUZZ_DUTY_HIGH - BUZZ_DUTY_LOW) * urgency
        if not buzzer_on and now >= next_beep_at:
            try: buzzer.write(duty); buzzer_on = True; buzz_off_at = now + BUZZ_ON_SEC; next_beep_at = now + interval
            except: pass
        elif buzzer_on and now >= buzz_off_at:
            try: buzzer.write(0); buzzer_on = False
            except: pass
    else:
        if buzzer_on:
            try: buzzer.write(0); buzzer_on = False
            except: pass

def compute_x_zone(face_px, frame_w, num_zones, current_zone, hysteresis):
    zone_w = frame_w / num_zones
    raw = max(0, min(num_zones - 1, int(face_px / zone_w)))
    if current_zone is None or abs(raw - current_zone) >= 2: return raw
    if raw == current_zone: return current_zone
    if raw == current_zone + 1:
        if face_px > (current_zone + 1) * zone_w + hysteresis: return current_zone + 1
    elif raw == current_zone - 1:
        if face_px < current_zone * zone_w - hysteresis: return current_zone - 1
    return current_zone

def zone_to_angle(zone, num_zones, a_min, a_max, invert):
    if num_zones <= 1: return (a_min + a_max) / 2
    norm = zone / (num_zones - 1)
    if invert: norm = 1 - norm
    return a_min + norm * (a_max - a_min)

def adaptive_smooth(prev, raw, base_alpha, fast_alpha, threshold):
    if prev is None: return raw
    delta = abs(raw - prev)
    if delta > threshold:
        t = min(1.0, (delta - threshold) / (threshold * 2))
        alpha = base_alpha + (fast_alpha - base_alpha) * t
    else: alpha = base_alpha
    return (1 - alpha) * prev + alpha * raw

def map_y_continuous(face_pos, frame_size, out_min, out_max, gain, invert):
    norm = (face_pos / frame_size) - 0.5
    norm = max(-0.5, min(0.5, norm * gain))
    if invert: norm = -norm
    return (norm + 0.5) * (out_max - out_min) + out_min

# ---------- CAMERA ----------
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")
cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, WS)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, HS)
cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
if not cap.isOpened():
    print("Camera not accessible."); board.exit(); raise SystemExit

# ---------- STATE ----------
target_x, target_y = float(HOME_X), float(HOME_Y)
last_x_sent, last_x_command_time, last_y_sent = None, 0.0, None
history, lock_start_time, last_seen_time = [], None, 0.0
fired_this_engagement = False; state = "IDLE"
smoothed_fx, smoothed_fy = WS / 2.0, HS / 2.0
last_raw_fx, last_raw_fy = None, None
current_x_zone = None; recentered_since_loss = True

def prune_history(now):
    cutoff = now - AIM_STABLE_WINDOW
    while history and history[0][0] < cutoff: history.pop(0)

def is_steady():
    if len(history) < 3: return False
    xs = [h[1] for h in history]; ys = [h[2] for h in history]
    return (max(xs) - min(xs) <= AIM_STABLE_DEG) and (max(ys) - min(ys) <= AIM_STABLE_DEG)

def write_x(angle, now, force=False):
    global last_x_sent, last_x_command_time
    angle = int(max(X_MIN, min(X_MAX, angle)))
    if not force:
        if last_x_sent is not None and angle == last_x_sent: return False
        if (now - last_x_command_time) < X_COMMAND_MIN_INTERVAL_SEC: return False
    try: servoX.write(angle); last_x_sent = angle; last_x_command_time = now; return True
    except: return False

def write_y(angle):
    global last_y_sent
    angle = int(max(Y_MIN, min(Y_MAX, angle)))
    if last_y_sent is not None and abs(angle - last_y_sent) < DEADBAND_DEG: return False
    try: servoY.write(angle); last_y_sent = angle; return True
    except: return False

# ---------- MAIN LOOP ----------
try:
    while True:
        ok, img = cap.read()
        if not ok: continue
        h_img, w_img = img.shape[:2]
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        small = cv2.resize(gray, (0, 0), fx=DETECT_SCALE, fy=DETECT_SCALE, interpolation=cv2.INTER_AREA)
        small_faces = face_cascade.detectMultiScale(small, scaleFactor=1.2, minNeighbors=5, minSize=(int(60 * DETECT_SCALE), int(60 * DETECT_SCALE)), flags=cv2.CASCADE_SCALE_IMAGE)
        inv = 1.0 / DETECT_SCALE
        faces = [(int(x*inv), int(y*inv), int(w*inv), int(h*inv)) for (x,y,w,h) in small_faces]

        now = time.time()
        face_detected = len(faces) > 0; used_detection = False

        if face_detected:
            x,y,w,h = max(faces, key=lambda f: f[2]*f[3])
            raw_fx, raw_fy = x + w//2, y + h//2
            outlier = last_raw_fx is not None and max(abs(raw_fx-last_raw_fx), abs(raw_fy-last_raw_fy)) > MAX_JUMP_PX
            if not outlier:
                smoothed_fx = adaptive_smooth(smoothed_fx, raw_fx, FACE_SMOOTH_X_BASE, FACE_SMOOTH_X_FAST, JUMP_THRESHOLD_PX)
                smoothed_fy = adaptive_smooth(smoothed_fy, raw_fy, FACE_SMOOTH_Y_BASE, FACE_SMOOTH_Y_FAST, JUMP_THRESHOLD_PX)
                new_zone = compute_x_zone(smoothed_fx, w_img, X_ZONE_COUNT, current_x_zone, X_ZONE_HYSTERESIS_PX)
                if new_zone != current_x_zone: print(f"X zone {current_x_zone} -> {new_zone}")
                current_x_zone = new_zone
                target_x = zone_to_angle(current_x_zone, X_ZONE_COUNT, X_MIN, X_MAX, INVERT_X)
                target_y = map_y_continuous(smoothed_fy, h_img, Y_MIN, Y_MAX, Y_GAIN, INVERT_Y)
                last_raw_fx, last_raw_fy = raw_fx, raw_fy
                last_seen_time = now; used_detection = True; recentered_since_loss = False
                if lock_start_time is None: lock_start_time = now
                cv2.rectangle(img, (x,y), (x+w,y+h), (0,255,0), 2)
                cv2.circle(img, (int(smoothed_fx), int(smoothed_fy)), 4, (0,255,255), -1)

        if not used_detection and (now - last_seen_time) >= REARM_LOST_SEC:
            if fired_this_engagement: print("Re-armed (face lost).")
            fired_this_engagement = False; lock_start_time = None; history.clear()
            last_raw_fx = last_raw_fy = None; current_x_zone = None
            if not recentered_since_loss: target_x = HOME_X; write_x(HOME_X, now, force=True); recentered_since_loss = True

        set_laser(True if used_detection else ((now - last_seen_time) < REARM_LOST_SEC and laser_on))
        if not used_detection and (now - last_seen_time) >= REARM_LOST_SEC: set_laser(False)

        write_x(target_x, now); write_y(target_y)
        if used_detection:
            sx_i, sy_i = int(max(X_MIN,min(X_MAX,target_x))), int(max(Y_MIN,min(Y_MAX,target_y)))
            history.append((now, sx_i, sy_i)); prune_history(now)

        if trigger_busy: state = "FIRING"
        elif not used_detection: state = "IDLE" if (now - last_seen_time) >= REARM_LOST_SEC else state
        elif fired_this_engagement: state = "LOCKED"
        else:
            held = now - (lock_start_time or now)
            if held >= AIM_DWELL_SEC and is_steady():
                fired_this_engagement = True
                threading.Thread(target=fire_trigger, daemon=True).start()
                state = "FIRING"
            else: state = "AIMING"

        if state == "AIMING" and lock_start_time:
            update_buzzer(True, min(1.0, (now - lock_start_time) / AIM_DWELL_SEC), now)
        elif state == "LOCKED": update_buzzer(True, 0.0, now)
        else: update_buzzer(False, 0.0, now)

        cv2.putText(img, state, (20,35), cv2.FONT_HERSHEY_PLAIN, 2, (0,200,255), 2)
        cv2.imshow("Turret", img)
        if cv2.waitKey(1) & 0xFF == ord("q"): break

finally:
    cap.release(); cv2.destroyAllWindows()
    try: set_laser(False); buzzer.write(0); servoX.write(HOME_X); servoY.write(HOME_Y); time.sleep(0.5)
    except: pass
    try: board.exit()
    except: pass
    print("Closed cleanly.")
