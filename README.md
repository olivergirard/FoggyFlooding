# Foggy Flooding
Foggy Flooding is an application that simulates the flooding of a city using ray tracing, fluid dynamics, Perlin noise, and procedural model generation. This application assists in the evaluation of flood impacts under various scenarios, as the density of the urban environment and the intensity of surrounding fog and water are adjustable. This application was constructed by Oliver Girard, Sei Zou, and Tongwei Zhang as a final project for CS43400: Advanced Computer Graphics. It makes use of ImGui and OpenGL with GLM, GLFW, and GLEW.

## Execution
Foggy Flooding can be run from the `CS434 Final Project.exe` file located in the `x64\Release` folder. This application can also be run from the included `CS434 Final Project.sln` file if all relevant files and libraries are linked; relevant libraries are in the `libs` folder, and relevant files are in the `include` folder.

## Features
### Interactivity
The number of buildings and range of building heights can be adjusted. The intensity of the fog can be adjusted. The number of water particles added each frame can be adjusted. The number of frames to render for the final animation can be adjusted, but more frames will necessitate more render time.
<p align="center">
  <img src="https://github.com/user-attachments/assets/78b39979-ab81-4568-845c-85085beab4df" alt="Adjustable parameters."/>
</p>

### Animation Rendering
After selecting the desired values for all parameters, each frame of the final animation is rendered and stored. 
<p align="center">
  <img src="https://github.com/user-attachments/assets/f6f27fe1-a33f-4df7-95fd-9adc9a9e177b" alt="Final animation."/>
</p>

When all frames are rendered, the final animation is played back repeatedly. Due to the randomness required to generate the buildings, fog, and water particles, no two animations will be the same.
<p align="center">
  <img src="https://github.com/user-attachments/assets/2de8a2ab-ffe5-4006-a6e4-1706e935540c" alt="Final animation."/>
</p>
