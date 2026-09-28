#include <SDL3/SDL.h>
#include <stdio.h>
#define N 1048576
int main(void){
  SDL_Init(SDL_INIT_VIDEO);
  SDL_GPUDevice*d=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV,false,NULL);
  size_t sz; void*code=SDL_LoadFile("relax.spv",&sz);
  SDL_GPUComputePipelineCreateInfo ci={0};
  ci.code=code; ci.code_size=sz; ci.entrypoint="main"; ci.format=SDL_GPU_SHADERFORMAT_SPIRV;
  ci.num_readwrite_storage_buffers=1; ci.threadcount_x=64; ci.threadcount_y=1; ci.threadcount_z=1;
  SDL_GPUComputePipeline*p=SDL_CreateGPUComputePipeline(d,&ci); if(!p){printf("pipe fail %s\n",SDL_GetError());return 1;}
  SDL_GPUBufferCreateInfo bi={.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,.size=N*4};
  SDL_GPUBuffer*b=SDL_CreateGPUBuffer(d,&bi);
  SDL_GPUTransferBufferCreateInfo ti={.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,.size=N*4};
  SDL_GPUTransferBuffer*t=SDL_CreateGPUTransferBuffer(d,&ti);
  Uint64 t0=SDL_GetTicksNS();
  SDL_GPUCommandBuffer*cb=SDL_AcquireGPUCommandBuffer(d);
  SDL_GPUStorageBufferReadWriteBinding bb={.buffer=b,.cycle=false};
  SDL_GPUComputePass*cp=SDL_BeginGPUComputePass(cb,NULL,0,&bb,1);
  SDL_BindGPUComputePipeline(cp,p); SDL_DispatchGPUCompute(cp,N/64,1,1); SDL_EndGPUComputePass(cp);
  SDL_GPUCopyPass*cpy=SDL_BeginGPUCopyPass(cb);
  SDL_GPUBufferRegion src={.buffer=b,.offset=0,.size=N*4}; SDL_GPUTransferBufferLocation dst={.transfer_buffer=t,.offset=0};
  SDL_DownloadFromGPUBuffer(cpy,&src,&dst); SDL_EndGPUCopyPass(cpy);
  SDL_GPUFence*f=SDL_SubmitGPUCommandBufferAndAcquireFence(cb); SDL_WaitForGPUFences(d,true,&f,1);
  Uint64 t1=SDL_GetTicksNS();
  Uint32*r=SDL_MapGPUTransferBuffer(d,t,false);
  int ok=1; for(int i=0;i<N;i++) if(r[i]!=(Uint32)i*2){ok=0;printf("mismatch at %d: %u\n",i,r[i]);break;}
  printf("compute %s, %d elems, dispatch+readback %.3f ms\n", ok?"OK":"FAIL", N,(t1-t0)/1e6);
  return !ok;}
