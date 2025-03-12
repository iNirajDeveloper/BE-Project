import cv2
import numpy as np
import os
import urllib.request

# Path to the dataset folder
dataset_path = 'dataset'

# Initialize the face detector
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

# Initialize the face recognizer
recognizer = cv2.face.LBPHFaceRecognizer_create()

# Function to load images and labels from the dataset
def get_images_and_labels(dataset_path):
    image_paths = [os.path.join(dataset_path, f) for f in os.listdir(dataset_path)]
    images = []
    labels = []
    label_map = {}  # Map names to unique integer labels
    current_label = 0  # Counter for assigning unique labels

    for image_path in image_paths:
        # Read the image and convert it to grayscale
        image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        
        # Extract the name from the filename (e.g., "john_0.jpg" -> "john")
        name = os.path.split(image_path)[-1].split("_")[0]
        
        # Assign a unique integer label to each name
        if name not in label_map:
            label_map[name] = current_label
            current_label += 1
        label = label_map[name]
        
        # Detect faces in the image
        faces = face_cascade.detectMultiScale(image, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))
        
        for (x, y, w, h) in faces:
            # Add the face ROI and label to the lists
            images.append(image[y:y+h, x:x+w])
            labels.append(label)
    
    return images, labels, label_map

# Load the dataset and train the recognizer
print("Loading dataset and training the model...")
images, labels, label_map = get_images_and_labels(dataset_path)
recognizer.train(images, np.array(labels))
recognizer.save('face_recognizer.yml')
print("Training complete. Model saved to 'face_recognizer.yml'.")

# Load the trained model
recognizer.read('face_recognizer.yml')

# Reverse the label_map to map labels to names
name_map = {v: k for k, v in label_map.items()}

# Initialize the ESP32-CAM stream
stream_url = 'http://172.29.50.76/cam-hi.jpg'
cv2.namedWindow("Live Cam Testing", cv2.WINDOW_AUTOSIZE)

# Function to detect and recognize faces
def detect_and_recognize_faces(frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))
    
    for (x, y, w, h) in faces:
        face_roi = gray[y:y+h, x:x+w]
        
        # Predict the label and confidence
        label, confidence = recognizer.predict(face_roi)
        
        # Get the name corresponding to the predicted label
        name = name_map.get(label, "Unknown")
        
        # Draw a rectangle around the face
        cv2.rectangle(frame, (x, y), (x+w, y+h), (255, 0, 0), 2)
        
        # Display the name and confidence
        cv2.putText(frame, f'{name} ({confidence:.2f})', (x, y-10), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (255, 0, 0), 2)
    
    return frame

# Main loop to read and display video frames
print("Starting live stream. Press 'q' to quit.")
while True:
    try:
        # Read a frame from the ESP32-CAM stream
        img_resp = urllib.request.urlopen(stream_url)
        imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
        frame = cv2.imdecode(imgnp, -1)
        
        # Detect and recognize faces
        frame = detect_and_recognize_faces(frame)
        
        # Display the frame
        cv2.imshow('Live Cam Testing', frame)
        
        # Break the loop if 'q' is pressed
        key = cv2.waitKey(1)
        if key == ord('q'):
            break
    except Exception as e:
        print(f"Error reading frame: {e}")
        break

cv2.destroyAllWindows()
