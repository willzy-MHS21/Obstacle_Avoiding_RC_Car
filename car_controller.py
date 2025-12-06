import serial
import keyboard
import pygame
import time
import sys

# Arduino's COM port 
ARDUINO_PORT = 'COM3'  
BAUD_RATE = 9600

# Controller settings
STICK_DEADZONE = 0.3
TRIGGER_THRESHOLD = 0.1

# Global variables
controller = None
controller_connected = False
last_command = None
button_pressed = {}
arduino = None

"""Check if a controller is connected and initialize it"""
def check_for_controller():
    global controller, controller_connected
    
    # If controller already connected, just verify it's still there
    if controller_connected and controller:
        try:
            if controller.get_init():
                return True
        except:
            pass
    
    # If no controller connected, reinit to detect new connections
    if not controller_connected:
        pygame.joystick.quit()
        pygame.joystick.init()
    
    joystick_count = pygame.joystick.get_count()
    
    if joystick_count > 0:
        if not controller_connected:
            # Initialize newly detected controller
            controller = pygame.joystick.Joystick(0)
            controller.init()
            controller_connected = True
            print(f"\nXbox Controller Connected: {controller.get_name()}")
        return True
    else:
        if controller_connected:
            print("\nXbox Controller Disconnected")
            if controller:
                controller.quit()
            controller = None
            controller_connected = False
        return False

"""Establish serial connection with Arduino"""
def connect_arduino():
    global arduino
    
    try:
        arduino = serial.Serial(ARDUINO_PORT, BAUD_RATE, timeout=1)
        time.sleep(2) 
        print(f"Connected to {ARDUINO_PORT}")
        return True
    except:
        print(f"ERROR: Could not connect to {ARDUINO_PORT}")
        return False

"""Display control instructions"""
def display_controls():
    print("\n" + "=" * 50)
    print("         KEYBOARD CONTROLS")
    print("=" * 50)
    print("  W - Move Forward")
    print("  S - Move Backward")
    print("  A - Turn Left")
    print("  D - Turn Right")
    print("  SPACE - Stop")
    print("  M - Auto Mode")
    print("  H - Horn/Buzzer")
    print("  ESC - Quit Program")

    if controller_connected:
        print("\n" + "=" * 50)
        print("        XBOX CONTROLLER")
        print("=" * 50)
        print("  TRIGGERS:")
        print("    RT  - Move Forward")
        print("    LT  - Move Backward")
        print("")
        print("  LEFT STICK:")
        print("    ← - Turn Left")
        print("    → - Turn Right")
        print("")
        print("  BUTTONS:")
        print("    A - Stop Car")
        print("    B - Exit Program")
        print("    X - Horn/Buzzer")
        print("    Y - Auto Mode")

    print("=" * 50)
    print("\nCar Ready! Use keyboard or controller...\n")

"""Send command to Arduino and provide feedback"""
def send_command(cmd, source=""):
    global last_command
    
    if cmd != last_command:
        arduino.write(cmd.encode())
        
        actions = {
            'W': '⬆️  Moving Forward',
            'S': '⬇️  Moving Backward',
            'A': '⬅️  Turning Left',
            'D': '➡️  Turning Right',
            'X': '⏹️  Stopped',
            'M': '🤖  Auto Mode Activated',
            'H': '📢  Beep!'
        }
        
        if cmd in actions:
            print(f"{actions[cmd]} {source}")
        
        # Check Arduino response
        if arduino.in_waiting > 0:
            response = arduino.readline().decode('utf-8').strip()
            print(f"[Arduino says]: {response}")
        
        last_command = cmd

"""Convert analog stick X-axis to turn command"""
def get_stick_command(x_axis):
    if x_axis < -STICK_DEADZONE:  # Left
        return 'A'
    elif x_axis > STICK_DEADZONE:  # Right
        return 'D'
    return None

"""Check keyboard input and return command"""
def check_keyboard():
    if keyboard.is_pressed('w'):
        return 'W', '(Keyboard)'
    elif keyboard.is_pressed('s'):
        return 'S', '(Keyboard)'
    elif keyboard.is_pressed('a'):
        return 'A', '(Keyboard)'
    elif keyboard.is_pressed('d'):
        return 'D', '(Keyboard)'
    elif keyboard.is_pressed('space'):
        return 'X', '(Keyboard)'
    elif keyboard.is_pressed('m'):
        time.sleep(0.3)  # Debounce
        return 'M', '(Keyboard)'
    elif keyboard.is_pressed('h'):
        time.sleep(0.3)  # Debounce
        return 'H', '(Keyboard)'
    elif keyboard.is_pressed('esc'):
        return 'ESC', '(Keyboard)'
    return None, ''

"""Check controller input and return command"""
def check_controller():
    global controller_connected, controller
    
    if not controller_connected or controller is None:
        return None, ''
    
    try:
        # Verify controller is still initialized
        if not controller.get_init():
            raise pygame.error("Controller not initialized")
        
        # Read triggers and analog stick
        right_trigger = controller.get_axis(5)  # RT - Forward
        left_trigger = controller.get_axis(4)   # LT - Backward
        left_x = controller.get_axis(0)         # Left stick horizontal
        
        # Normalize triggers to 0-1 range
        rt_value = (right_trigger + 1) / 2
        lt_value = (left_trigger + 1) / 2
        
        # Triggers for forward/backward
        if rt_value > TRIGGER_THRESHOLD:
            return 'W', '(Controller)'
        elif lt_value > TRIGGER_THRESHOLD:
            return 'S', '(Controller)'
        
        # If no triggers, check stick for turning
        turn_cmd = get_stick_command(left_x)
        if turn_cmd:
            return turn_cmd, '(Controller)'
        
        return None, ''
    
    except (pygame.error, AttributeError) as e:
        # Controller disconnected or error occurred
        print(f"\nController error: {e}")
        controller_connected = False
        controller = None
        return None, ''

"""Handle pygame events for controller connection and buttons"""
def handle_pygame_events():
    global controller, controller_connected
    
    command = None
    source = ''
    
    for event in pygame.event.get():
        if event.type == pygame.JOYDEVICEADDED:
            # Controller was just plugged in
            if not controller_connected:
                controller = pygame.joystick.Joystick(event.device_index)
                controller.init()
                controller_connected = True
                print(f"\nXbox Controller Connected: {controller.get_name()}")
        
        elif event.type == pygame.JOYDEVICEREMOVED:
            # Controller was unplugged
            if controller_connected:
                print("\nXbox Controller Disconnected")
                controller = None
                controller_connected = False
        
        elif event.type == pygame.JOYBUTTONDOWN and controller_connected:
            button = event.button
            
            if button == 0:  # A button - Stop
                command = 'X'
                source = '(Controller)'
            elif button == 1:  # B button - Exit
                command = 'ESC'
                source = '(Controller)'
            elif button == 2:  # X button - Horn
                command = 'H'
                source = '(Controller)'
            elif button == 3:  # Y button - Auto Mode
                command = 'M'
                source = '(Controller)'
    
    return command, source

"""Main program loop"""
def main():
    global last_command
    
    # Initialize pygame for controller support
    pygame.init()
    pygame.joystick.init()

    print("=" * 50)
    print("  OBSTACLE AVOIDING RC CAR")
    print("=" * 50)

    # Initial controller check
    check_for_controller()
    if controller_connected:
        print(f"Xbox Controller: {controller.get_name()}")
    else:
        print("Xbox Controller: Not detected")

    print("Connecting to Arduino...")

    # Connect to Arduino
    if not connect_arduino():
        if controller_connected:
            pygame.quit()
        sys.exit()

    # Display controls
    display_controls()

    try:
        clock = pygame.time.Clock()
        running = True
        
        while running:
            command = None
            source = ''
            
            # Handle pygame events (controller connect/disconnect/buttons)
            event_command, event_source = handle_pygame_events()
            if event_command:
                command = event_command
                source = event_source
            
            # Check controller axes (triggers/sticks) if connected
            if not command and controller_connected:
                controller_command, controller_source = check_controller()
                if controller_command:
                    command = controller_command
                    source = controller_source
            
            # If no controller command, check keyboard
            if not command:
                command, source = check_keyboard()
            
            # Handle exit command
            if command == 'ESC':
                print(f"\nStopping car and exiting... {source}")
                arduino.write(b'X')  # Stop the car
                time.sleep(0.5)
                break
            
            # Send command if we have one
            if command:
                send_command(command, source)
            elif last_command not in ['M', 'H', 'X']:
                # Auto-stop when no input (except in special modes)
                send_command('X', '')
            
            # Maintain update rate
            clock.tick(20)  # 20 Hz
            time.sleep(0.05)

    except KeyboardInterrupt:
        print("\n\nProgram interrupted. Stopping car...")
        arduino.write(b'X')
        time.sleep(0.5)

    finally:
        # Clean up
        arduino.close()
        if controller_connected and controller:
            controller.quit()
        pygame.quit()
        print("Disconnected from Arduino")
        print("Goodbye!")

if __name__ == "__main__":
    main()