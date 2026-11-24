// Included inside the preview's anonymous namespace, after Texture is defined.
// SDL owns the OpenGL context; stage projection matches the Direct3D backend.
struct StageGpu {
    std::map<std::string, unsigned> textures;
    GLuint framebuffer=0, target=0, depth_buffer=0;
    std::vector<unsigned> readback;
    StageGpu(int width,int height,const std::vector<skullgirls::StageMesh>&,
             const std::map<std::string, skullgirls::SpriteBytes>& sources) {
        if(!SDL_GL_GetCurrentContext()) throw std::runtime_error("No OpenGL stage context");
        if(!SDL_GL_GetProcAddress("glGenFramebuffers")) throw std::runtime_error("OpenGL framebuffer support is unavailable");
        try {
            glGenFramebuffers(1,&framebuffer); glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
            glGenTextures(1,&target); glBindTexture(GL_TEXTURE_2D,target);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_BGRA,GL_UNSIGNED_BYTE,nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);
            glGenRenderbuffers(1,&depth_buffer); glBindRenderbuffer(GL_RENDERBUFFER,depth_buffer);
            glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth_buffer);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error("Cannot allocate OpenGL stage framebuffer");
            glBindFramebuffer(GL_FRAMEBUFFER,0);
            readback.resize(std::size_t(width)*height);
            for(const auto& [name,bytes]:sources) {
                const auto layout=Texture::dds_layout(bytes);
                auto& texture=textures[name];
                glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
                const auto* extensions=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
                if(layout.width%4==0 && layout.height%4==0 && extensions &&
                   std::strstr(extensions,"GL_EXT_texture_compression_s3tc")) {
                    const auto format=layout.block_size==16?GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                    glCompressedTexImage2D(GL_TEXTURE_2D,0,format,layout.width,layout.height,0,
                        ((layout.width+3)/4)*((layout.height+3)/4)*layout.block_size,bytes.data()+128);
                } else {
                    std::vector<unsigned> pixels(layout.width*layout.height);
                    Texture::decode_dds(bytes,layout,Texture::Encoding::straight_rgba,1,pixels.data());
                    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,layout.width,layout.height,0,GL_BGRA,GL_UNSIGNED_BYTE,pixels.data());
                }
            }
            if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("OpenGL stage texture upload failed");
        } catch(...) { release(); throw; }
    }
    void release() {
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        for(auto& entry:textures) glDeleteTextures(1,&entry.second);
        textures.clear();
        if(target) glDeleteTextures(1,&target);
        if(depth_buffer) glDeleteRenderbuffers(1,&depth_buffer);
        if(framebuffer) glDeleteFramebuffers(1,&framebuffer);
    }
    ~StageGpu() { release(); }
    void draw(int width,int height,const std::vector<skullgirls::StageMesh>& meshes,
              const std::vector<skullgirls::StageUv>& uv,float center,float view_height,
              float bottom,float distance,float focal,float near_clip,float far_clip,
              std::vector<unsigned>& color) {
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
        struct Unbind { ~Unbind(){glBindFramebuffer(GL_FRAMEBUFFER,0);} } unbind;
        glViewport(0,0,width,height);
        glClearColor(174.f/255,185.f/255,199.f/255,1);
        glDepthMask(GL_TRUE); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glDisable(GL_CULL_FACE); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
        glEnable(GL_TEXTURE_2D); glEnable(GL_ALPHA_TEST);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        const double right=width*near_clip/(2*focal),top=height*near_clip/(2*focal);
        glFrustum(-right,right,-top,top,near_clip,far_clip);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glTranslatef(-center,-(bottom+view_height*.5f),-distance);
        for(bool opaque:{true,false}) {
            glDepthMask(opaque?GL_TRUE:GL_FALSE);
            if(opaque) { glDisable(GL_BLEND); glAlphaFunc(GL_GEQUAL,.9999f); }
            else { glEnable(GL_BLEND); glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ZERO); glAlphaFunc(GL_LESS,.9999f); }
            for(std::size_t i=0;i<meshes.size();++i) {
                const auto& mesh=meshes[i]; const auto& transform=uv[i];
                glBindTexture(GL_TEXTURE_2D,textures.at(mesh.material));
                const auto filter=mesh.animates_uv()?GL_LINEAR:GL_NEAREST;
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,mesh.wrap_u?GL_REPEAT:GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,mesh.wrap_v?GL_REPEAT:GL_CLAMP_TO_EDGE);
                // Skip the fully opaque texels on the translucent pass, matching
                // the preview's two-pass depth behavior without duplicating them.
                glBegin(GL_TRIANGLES);
                for(const auto index:mesh.indices) {
                    const auto& v=mesh.vertices[index];
                    glColor4ub(v.rgba>>24,(v.rgba>>16)&255,(v.rgba>>8)&255,v.rgba&255);
                    glTexCoord2f(v.u*transform[0]+v.v*transform[1]+transform[2],
                        1-(v.u*transform[3]+v.v*transform[4]+transform[5]));
                    glVertex3f(v.x,v.y,v.z);
                }
                glEnd();
            }
        }
        glDepthMask(GL_TRUE); glDisable(GL_BLEND); glDisable(GL_ALPHA_TEST); glDisable(GL_DEPTH_TEST);
        glReadPixels(0,0,width,height,GL_BGRA,GL_UNSIGNED_BYTE,readback.data());
        if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("OpenGL stage rendering failed");
        for(int y=0;y<height;++y) {
            auto* dst=color.data()+y*width;
            std::copy_n(readback.data()+(height-1-y)*width,width,dst);
            for(int x=0;x<width;++x) dst[x]|=0xff000000u;
        }
    }
};
