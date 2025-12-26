import cv2
import pygame
import os
import time
import numpy as np
import csv

# 47 er dum

video_path = "IMG_6645.MOV" 
DEADZONE = 0.05 # Fordi mit joystick drifter - lidt ligegyldigt efter bins tilføjelse
MAX_ANGLE = 30.0          # maks steering angle       
OUTPUT_DIR = "dataset"


# output image size
OUT_WIDTH = 32
OUT_HEIGHT = 32

os.makedirs(OUTPUT_DIR, exist_ok=True)

cap = cv2.VideoCapture(video_path)

delay = int(1000 / cap.get(cv2.CAP_PROP_FPS)) # ms per frame

window_name = "Video + L3"
cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)

# joystick setup-
pygame.init()
pygame.joystick.init()

if pygame.joystick.get_count() == 0:
    print("No joystick")
    quit()

js = pygame.joystick.Joystick(0)
# js.rumble(500, 500,2000)


print(f"Controller: {js.get_name()}")
print("'q' to quit")

frame_index = 0

first_frame = True

while True:
    ret, raw_frame = cap.read()
    if not ret:
        print("Done")
        break

    # frame_out = cv2.resize(raw_frame, (OUT_WIDTH, OUT_HEIGHT), interpolation=cv2.INTER_AREA)



    if first_frame:
        first_frame = False
        cv2.imshow(window_name, raw_frame)  # Show first frame for a second
        cv2.resizeWindow(window_name, 1200, 800)
        cv2.waitKey(1000)

    pygame.event.pump()
    steering = js.get_axis(0)  # -1 (venstre) til 1 (højre)

    if abs(steering) < DEADZONE:
        steering = 0.0
    
    steering_deg = steering * MAX_ANGLE

    bin_value = int(round(steering_deg/10.0)*10.0) # round to nearest 5
    
    bin_folder = os.path.join(OUTPUT_DIR, f"{bin_value}")
    os.makedirs(bin_folder, exist_ok=True)


    # gray = cv2.cvtColor(frame_out, cv2.COLOR_BGR2GRAY)
    # (thresh, blackAndWhiteImage) = cv2.threshold(gray, 127, 255, cv2.THRESH_BINARY)
    
    filename = os.path.join(bin_folder, f"frame_{time.time()}.jpg")
    # cv2.imwrite(filename, frame_out)
    cv2.imwrite(filename, raw_frame)

    # Play video with steering angle shown
    frame_display = raw_frame.copy()

    text_deg = f"Angle: {steering_deg:+.1f}  (bin: {bin_value:+d})"
    org = (20, 40)

    # sort outline
    cv2.putText(frame_display, text_deg, org,
                cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 0), 4)
    # grøn tekst
    cv2.putText(frame_display, text_deg, org,
                cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 0), 2)

    cv2.imshow(window_name, frame_display)

    cv2.resizeWindow(window_name, 1200, 800)

    frame_index += 1

    # if frame_index == 20:
    #     save_frame = cv2.resize(raw_frame, (32, 32), interpolation=cv2.INTER_AREA)
    #     save_frame = cv2.cvtColor(save_frame, cv2.COLOR_BGR2GRAY)
    #     save_frame = save_frame.astype(np.float32) / 255.0

    #     flat_input = save_frame.flatten()
    #     np.set_printoptions(threshold=np.inf)

    #     print("bin: ", bin_value, " array: ", flat_input)

    # if frame_index == 40:
    #     save_frame = cv2.resize(raw_frame, (32, 32), interpolation=cv2.INTER_AREA)
    #     save_frame = cv2.cvtColor(save_frame, cv2.COLOR_BGR2GRAY)
    #     save_frame = save_frame.astype(np.float32) / 255.0

    #     flat_input = save_frame.flatten()
    #     print("bin: ", bin_value, " array: ", flat_input)



    if cv2.waitKey(delay) & 0xFF == ord('q'):
        break


cap.release()
cv2.destroyAllWindows()
pygame.quit()
