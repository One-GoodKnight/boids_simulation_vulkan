![1 million boids simulation Vulkan](https://youtu.be/5QUGGlCqLzM)
  
Boids  
- separation: steer to avoid crowding local flockmates  
- alignment: steer towards the average heading of local flockmates  
- cohesion: steer to move towards the average position (center of mass) of local flockmates  
  
The simulation has been optimized with a spatial hash grid using blelloch algo for the scan.  
Instead of every boid chechink every other boid, we put each boid in a cell at the beginning of  
the frame and when needing to access neighbors, we can get directly those within the same cell.
  
Both the spatial hash grid and the simulation run on the GPU using compute shaders.  
  
Color of the boids represents their velocity.  
