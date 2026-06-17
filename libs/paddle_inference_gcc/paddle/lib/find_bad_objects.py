import subprocess
import os

# Get all objects
result = subprocess.run(['D:/PaddleOCR_MinGW/mingw/bin/ar.exe', 't', 'libpaddle_inference.a'], 
                       capture_output=True, text=True)
objects = result.stdout.strip().split('\n')

bad_objects = []
for obj in objects:
    obj = obj.strip()
    if not obj:
        continue
    
    # Extract object
    subprocess.run(['D:/PaddleOCR_MinGW/mingw/bin/ar.exe', 'x', 'libpaddle_inference.a', obj], 
                  capture_output=True)
    
    # Check for ThreadDataRegistry
    nm_result = subprocess.run(['D:/PaddleOCR_MinGW/mingw/bin/nm.exe', obj], 
                              capture_output=True, text=True)
    if 'ThreadDataRegistry' in nm_result.stdout or 'HostMemoryStatReserved' in nm_result.stdout:
        bad_objects.append(obj)
    
    # Clean up
    if os.path.exists(obj):
        os.remove(obj)

print(f"Found {len(bad_objects)} objects with ThreadDataRegistry:")
for obj in bad_objects:
    print(f"  {obj}")
