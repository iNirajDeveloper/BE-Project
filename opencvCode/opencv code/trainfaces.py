import cv2
import urllib.request
import numpy as np
import os

# Replace the URL with the IP camera's stream URL
url = 'http://172.29.50.76/cam-hi.jpg'

# Path to the directory where training images will be saved
dataset_path = 'dataset'
if not os.path.exists(dataset_path):
    os.makedirs(dataset_path)

# Initialize the face detector
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

# Counter to keep track of the number of saved images
image_counter = 0

# Function to save face images
def save_face_image(face, label):
    global image_counter
    image_name = f"person{label}.{image_counter}.jpg"
    image_path = os.path.join(dataset_path, image_name)
    cv2.imwrite(image_path, face)
    print(f"Saved {image_path}")
    image_counter += 1

# Main loop to capture and save images
print("Press 's' to save the detected face. Press 'q' to quit.")
while True:
    # Read a frame from the video stream
    img_resp = urllib.request.urlopen(url)
    imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
    frame = cv2.imdecode(imgnp, -1)
    
    # Convert the frame to grayscale for face detection
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    
    # Detect faces in the frame
    faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))
    
    # Draw rectangles around the detected faces and display the frame
    for (x, y, w, h) in faces:
        cv2.rectangle(frame, (x, y), (x+w, y+h), (255, 0, 0), 2)
    
    # Display the frame
    cv2.imshow('ESP32-CAM Stream', frame)
    
    # Wait for key press
    key = cv2.waitKey(1) & 0xFF
    
    # Save the detected face when 's' is pressed
    if key == ord('s'):
        if len(faces) == 1:
            (x, y, w, h) = faces[0]
            face_roi = gray[y:y+h, x:x+w]
            label = 1  # Assign a label (e.g., 1 for the first person)
            save_face_image(face_roi, label)
        else:
            print("No face or multiple faces detected. Please ensure only one face is visible.")
    
    # Break the loop if 'q' is pressed
    if key == ord('q'):
        break

cv2.destroyAllWindows()
