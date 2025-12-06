import serial
import keyboard
import time

# Arduino's COM port 
ARDUINO_PORT = 'COM3'  
BAUD_RATE = 9600

print("=" * 50)
print("      ROBOT CAR REAL-TIME CONTROLLER")
print("=" * 50)
print("Connecting to Arduino...")

# Establish serial connection with Arduino
try:
    # Open serial port with 1 second timeout
    arduino = serial.Serial(ARDUINO_PORT, BAUD_RATE, timeout=1)

    # Wait for Arduino to reset
    time.sleep(2) 

    print(f"✓ Connected to {ARDUINO_PORT}")
except:
    # Connection failed
    print(f"✗ ERROR: Could not connect to {ARDUINO_PORT}")
    exit()

# Display control instructions on the console
print("\n" + "=" * 50)
print("           CONTROLS (No Enter needed!)")
print("=" * 50)
print("  W - Move Forward")
print("  S - Move Backward")
print("  A - Turn Left")
print("  D - Turn Right")
print("  SPACE - Stop")
print("  M - Auto Mode (Obstacle Avoidance)")
print("  H - Horn/Buzzer")
print("  ESC - Quit Program")
print("=" * 50)
print("\n🎮 Ready! Press keys to control the car...")
print("   (Make sure this window is in focus!)\n")

# Track the last command sent to avoid sending duplicate commands
last_command = None

try:
    while True:
        command = None
        
        # Check which key is pressed
        if keyboard.is_pressed('w'):
            command = 'W'
        elif keyboard.is_pressed('s'):
            command = 'S'
        elif keyboard.is_pressed('a'):
            command = 'A'
        elif keyboard.is_pressed('d'):
            command = 'D'
        elif keyboard.is_pressed('space'):
            command = 'X'
        elif keyboard.is_pressed('m'):
            command = 'M'
            time.sleep(0.3)  # Debounce for mode switch
        elif keyboard.is_pressed('h'):
            command = 'H'
            time.sleep(0.3)  # Debounce for horn
        elif keyboard.is_pressed('esc'):
            print("\n🛑 Stopping car and exiting...")
            arduino.write(b'X')  # Stop the car
            time.sleep(0.5)
            break
        
        # Send command to Arduino if it changed
        if command and command != last_command:
            arduino.write(command.encode())
            print(f"[DEBUG] Sent '{command}' to Arduino")  # Debug message
            
            # Print feedback
            actions = {
                'W': '⬆️  Moving Forward',
                'S': '⬇️  Moving Backward',
                'A': '⬅️  Turning Left',
                'D': '➡️  Turning Right',
                'X': '⏹️  Stopped',
                'M': '🤖 Auto Mode Activated',
                'H': '📢 Beep!'
            }
            if command in actions:
                print(actions[command])
            
            # Check if Arduino sends anything back
            if arduino.in_waiting > 0:
                response = arduino.readline().decode('utf-8').strip()
                print(f"[Arduino says]: {response}")
            
            # Update last command tracker
            last_command = command
        
        time.sleep(0.05)  # Small delay to prevent overwhelming the Arduino

 # Handle Ctrl+C or ESC key interruption
except KeyboardInterrupt:
    print("\n\n🛑 Program interrupted. Stopping car...")
    arduino.write(b'X')
    time.sleep(0.5)

# Clean up the serial connection
finally:
    arduino.close()
    print("✓ Disconnected from Arduino")
    print("Goodbye! 👋")