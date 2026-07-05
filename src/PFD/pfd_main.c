#include <stdio.h>
#include "pfd_ui.h"
#include "pfd_data.h"

int main(int argc,char* argv[]){
    
    if (initPFD()==-1){
        return -1;
    }
    printf("PFD库初始化成功\n");

    SDL_Window* window=createPFDWindow();
    if (window==NULL){
        destroyPFD(window,NULL,NULL);
        return -1;
    }
    printf("PFD窗口创建成功\n");

    SDL_Renderer* renderer=createPFDRenderer(window);
    if (renderer==NULL){
        destroyPFD(window,renderer,NULL);
        return -1;
    }
    printf("PFD渲染器创建成功\n");

    int quit=0;
    SDL_Event event;

    logic_texture=createLogicTexture(renderer);
    SDL_SetRenderTarget(renderer,logic_texture);//切换到logic_texture纹理

    while (!quit){
        //事件监视
        while (SDL_PollEvent(&event)){
            if (event.type==SDL_QUIT) quit=1;
            if (event.type==SDL_KEYDOWN&&event.key.keysym.sym==SDLK_ESCAPE) quit=1;
            if (event.type==SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED){
                // 更新当前窗口物理尺寸
                window_width = event.window.data1;
                window_height = event.window.data2;

                // 计算缩放比例，保证画面完整显示且居中
                float scale_x=(float)window_width/772.0f;
                float scale_y=(float)window_height/721.0f;
                float scale=(scale_x<scale_y)?scale_x:scale_y;

                int render_w=(int)(772.0f*scale);
                int render_h=(int)(721.0f*scale);

                g_render_rect.x=(window_width-render_w)/2;
                g_render_rect.y=(window_height-render_h)/2;
                g_render_rect.w=render_w;
                g_render_rect.h=render_h;
            }
        }
        // 切换到逻辑纹理并清除
        SDL_SetRenderTarget(renderer, logic_texture);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);



        

        // 切换回窗口纹理，缩放渲染
        SDL_SetRenderTarget(renderer, NULL);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, logic_texture, NULL, &g_render_rect);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    destroyPFD(window, renderer,logic_texture);
    printf("PFD程序正常退出\n");
    return 0;
}
