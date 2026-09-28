#include <SDL3/SDL.h>
#include <stdio.h>
int main(void){
  if(!SDL_Init(SDL_INIT_VIDEO)){ printf("init fail %s\n",SDL_GetError()); }
  SDL_GPUDevice*d=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV,false,NULL);
  if(!d){printf("no device: %s\n",SDL_GetError());return 1;}
  printf("driver=%s formats=0x%x\n",SDL_GetGPUDeviceDriver(d),SDL_GetGPUShaderFormats(d));
  SDL_PropertiesID p=SDL_GetGPUDeviceProperties(d);
  printf("name=%s\n", SDL_GetStringProperty(p,SDL_PROP_GPU_DEVICE_NAME_STRING,"?"));
  printf("drvname=%s ver=%s\n", SDL_GetStringProperty(p,SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING,"?"),SDL_GetStringProperty(p,SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING,"?"));
  SDL_DestroyGPUDevice(d); return 0;}
