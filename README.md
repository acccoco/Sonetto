This is my real-time renderer, which I use to learn Rulkan and real-time rendering techniques.

### Todo List

- [x] learn vulkan tutorial
- [x] application base framework
- [x] hello triangle
- [x] mass and mipmap
- [ ] compute shader:
    - [x] particles
- [ ] tesselation shader
    - [x] height map based terrain
- [ ] shadow map
- [ ] pcss, pcf
- [ ] ssao, ssdo
- [ ] ssr

### Compute Shader

compute shader + particles 实现简单的曲面动画
![ripple](./doc/compute%20shader%20particle/ripple.jpg)
![sphere](./doc/compute%20shader%20particle/sphere.jpg)
![multi wave](./doc/compute%20shader%20particle/multi%20wave.jpg)

### Tesselation Shader

利用冰岛的高度图和 tesselation shader 生成地形
![terrain](./doc/tesselation_terrain/terrain.jpg)
![wireframe](./doc/tesselation_terrain/wireframe.jpg)
