#include "context.h"

C3D_Context __C3D_Context;

static void C3Di_SetTex(GPU_TEXUNIT unit, C3D_Tex* tex)
{
	u32 reg[4];
	reg[0] = tex->fmt;
	reg[1] = osConvertVirtToPhys(tex->data) >> 3;
	reg[2] = (u32)tex->height | ((u32)tex->width << 16);
	reg[3] = tex->param;

	switch (unit)
	{
		case GPU_TEXUNIT0:
			GPUCMD_AddWrite(GPUREG_TEXUNIT0_TYPE, reg[0]);
			GPUCMD_AddWrite(GPUREG_TEXUNIT0_ADDR1, reg[1]);
			GPUCMD_AddWrite(GPUREG_TEXUNIT0_DIM, reg[2]);
			GPUCMD_AddWrite(GPUREG_TEXUNIT0_PARAM, reg[3]);
			break;
		case GPU_TEXUNIT1:
			GPUCMD_AddWrite(GPUREG_TEXUNIT1_TYPE, reg[0]);
			GPUCMD_AddWrite(GPUREG_TEXUNIT1_ADDR, reg[1]);
			GPUCMD_AddWrite(GPUREG_TEXUNIT1_DIM, reg[2]);
			GPUCMD_AddWrite(GPUREG_TEXUNIT1_PARAM, reg[3]);
			break;
		case GPU_TEXUNIT2:
			GPUCMD_AddWrite(GPUREG_TEXUNIT2_TYPE, reg[0]);
			GPUCMD_AddWrite(GPUREG_TEXUNIT2_ADDR, reg[1]);
			GPUCMD_AddWrite(GPUREG_TEXUNIT2_DIM, reg[2]);
			GPUCMD_AddWrite(GPUREG_TEXUNIT2_PARAM, reg[3]);
			break;
	}
}

static aptHookCookie hookCookie;

static void C3Di_AptEventHook(APT_HookType hookType, void* param)
{
	C3D_Context* ctx = C3Di_GetContext();

	switch (hookType)
	{
		case APTHOOK_ONSUSPEND:
		{
			if (ctx->renderQueueWaitDone)
				ctx->renderQueueWaitDone();
			break;
		}
		case APTHOOK_ONRESTORE:
		{
			ctx->flags |= C3DiF_AttrInfo | C3DiF_BufInfo | C3DiF_Effect | C3DiF_RenderBuf
				| C3DiF_Viewport | C3DiF_Scissor | C3DiF_Program | C3DiF_VshCode | C3DiF_GshCode
				| C3DiF_TexAll | C3DiF_TexEnvBuf | C3DiF_TexEnvAll | C3DiF_LightEnv;

			C3Di_DirtyUniforms(GPU_VERTEX_SHADER);
			C3Di_DirtyUniforms(GPU_GEOMETRY_SHADER);

			ctx->fixedAttribDirty |= ctx->fixedAttribEverDirty;

			C3D_LightEnv* env = ctx->lightEnv;
			if (env)
				env->Dirty(env);
			break;
		}
		default:
			break;
	}
}

bool C3D_Init(size_t cmdBufSize)
{
	int i;
	C3D_Context* ctx = C3Di_GetContext();

	if (ctx->flags & C3DiF_Active)
		return false;

	ctx->cmdBufSize = cmdBufSize/8; // Half of the size of the cmdbuf, in words
	ctx->cmdBuf = (u32*)linearAlloc(cmdBufSize);
	ctx->cmdBufUsage = 0;
	if (!ctx->cmdBuf) return false;

	GPUCMD_SetBuffer(ctx->cmdBuf, ctx->cmdBufSize, 0);

	ctx->flags = C3DiF_Active | C3DiF_TexEnvBuf | C3DiF_TexEnvAll | C3DiF_Effect | C3DiF_TexAll;
	ctx->renderQueueExit = NULL;

	// TODO: replace with direct struct access
	C3D_DepthMap(-1.0f, 0.0f);
	C3D_CullFace(GPU_CULL_BACK_CCW);
	C3D_StencilTest(false, GPU_ALWAYS, 0x00, 0xFF, 0x00);
	C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
	C3D_BlendingColor(0);
	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
	C3D_AlphaTest(false, GPU_ALWAYS, 0x00);
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
	C3D_FragOpMode(GPU_FRAGOPMODE_GL);

	ctx->texEnvBuf = 0;
	ctx->texEnvBufClr = 0xFFFFFFFF;

	for (i = 0; i < 3; i ++)
		ctx->tex[i] = NULL;

	for (i = 0; i < 6; i ++)
		TexEnv_Init(&ctx->texEnv[i]);

	ctx->fixedAttribDirty = 0;
	ctx->fixedAttribEverDirty = 0;

	aptHook(&hookCookie, C3Di_AptEventHook, NULL);

	return true;
}

void C3D_SetViewport(u32 x, u32 y, u32 w, u32 h)
{
	C3D_Context* ctx = C3Di_GetContext();
	ctx->flags |= C3DiF_Viewport | C3DiF_Scissor;
	ctx->viewport[0] = f32tof24(w / 2.0f);
	ctx->viewport[1] = f32tof31(2.0f / w) << 1;
	ctx->viewport[2] = f32tof24(h / 2.0f);
	ctx->viewport[3] = f32tof31(2.0f / h) << 1;
	ctx->viewport[4] = (y << 16) | (x & 0xFFFF);
	ctx->scissor[0] = GPU_SCISSOR_DISABLE;
}

void C3D_SetScissor(GPU_SCISSORMODE mode, u32 left, u32 top, u32 right, u32 bottom)
{
	C3D_Context* ctx = C3Di_GetContext();
	ctx->flags |= C3DiF_Scissor;
	ctx->scissor[0] = mode;
	if (mode == GPU_SCISSOR_DISABLE) return;
	ctx->scissor[1] = (top << 16) | (left & 0xFFFF);
	ctx->scissor[2] = ((bottom-1) << 16) | ((right-1) & 0xFFFF);
}

void C3Di_UpdateContext(void)
{
	int i;
	C3D_Context* ctx = C3Di_GetContext();

	if (ctx->flags & C3DiF_Program)
	{
		shaderProgramConfigure(ctx->program, (ctx->flags & C3DiF_VshCode) != 0, (ctx->flags & C3DiF_GshCode) != 0);
		ctx->flags &= ~(C3DiF_Program | C3DiF_VshCode | C3DiF_GshCode);
	}

	if (ctx->flags & C3DiF_RenderBuf)
	{
		ctx->flags &= ~C3DiF_RenderBuf;
		if (ctx->flags & C3DiF_DrawUsed)
		{
			ctx->flags &= ~C3DiF_DrawUsed;
			GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);
			GPUCMD_AddWrite(GPUREG_EARLYDEPTH_CLEAR, 1);
		}
		C3Di_RenderBufBind(ctx->rb);
	}

	if (ctx->flags & C3DiF_Viewport)
	{
		ctx->flags &= ~C3DiF_Viewport;
		GPUCMD_AddIncrementalWrites(GPUREG_VIEWPORT_WIDTH, ctx->viewport, 4);
		GPUCMD_AddWrite(GPUREG_VIEWPORT_XY, ctx->viewport[4]);
	}

	if (ctx->flags & C3DiF_Scissor)
	{
		ctx->flags &= ~C3DiF_Scissor;
		GPUCMD_AddIncrementalWrites(GPUREG_SCISSORTEST_MODE, ctx->scissor, 3);
	}

	if (ctx->flags & C3DiF_AttrInfo)
	{
		ctx->flags &= ~C3DiF_AttrInfo;
		C3Di_AttrInfoBind(&ctx->attrInfo);
	}

	if (ctx->flags & C3DiF_BufInfo)
	{
		ctx->flags &= ~C3DiF_BufInfo;
		C3Di_BufInfoBind(&ctx->bufInfo);
	}

	if (ctx->flags & C3DiF_Effect)
	{
		ctx->flags &= ~C3DiF_Effect;
		C3Di_EffectBind(&ctx->effect);
	}

	if (ctx->flags & C3DiF_TexAll)
	{
		GPU_TEXUNIT units = 0;

		for (i = 0; i < 3; i ++)
		{
			static const u8 parm[] = { GPU_TEXUNIT0, GPU_TEXUNIT1, GPU_TEXUNIT2 };

			if (ctx->tex[i])
			{
				units |= parm[i];
				if (ctx->flags & C3DiF_Tex(i))
					C3Di_SetTex(parm[i], ctx->tex[i]);
			}
		}

		ctx->flags &= ~C3DiF_TexAll;
		GPUCMD_AddWrite(GPUREG_TEXUNIT_CONFIG, 0x00011000|units); // Enable texture units
	}

	if (ctx->flags & C3DiF_TexEnvBuf)
	{
		ctx->flags &= ~C3DiF_TexEnvBuf;
		GPUCMD_AddMaskedWrite(GPUREG_TEXENV_UPDATE_BUFFER, 0x2, ctx->texEnvBuf);
		GPUCMD_AddWrite(GPUREG_TEXENV_BUFFER_COLOR, ctx->texEnvBufClr);
	}

	if (ctx->flags & C3DiF_TexEnvAll)
	{
		for (i = 0; i < 6; i ++)
		{
			if (!(ctx->flags & C3DiF_TexEnv(i))) continue;
			C3Di_TexEnvBind(i, &ctx->texEnv[i]);
		}
		ctx->flags &= ~C3DiF_TexEnvAll;
	}

	C3D_LightEnv* env = ctx->lightEnv;

	if (ctx->flags & C3DiF_LightEnv)
	{
		u32 enable = env != NULL;
		GPUCMD_AddWrite(GPUREG_LIGHTING_ENABLE0, enable);
		GPUCMD_AddWrite(GPUREG_LIGHTING_ENABLE1, !enable);
		ctx->flags &= ~C3DiF_LightEnv;
	}

	if (env)
		env->Update(env);

	if (ctx->fixedAttribDirty)
	{
		for (i = 0; i < 12; i ++)
		{
			if (!(ctx->fixedAttribDirty & BIT(i))) continue;
			C3D_FVec* v = &ctx->fixedAttribs[i];

			GPUCMD_AddWrite(GPUREG_FIXEDATTRIB_INDEX, i);
			C3D_ImmSendAttrib(v->x, v->y, v->z, v->w);
		}
		ctx->fixedAttribDirty = 0;
	}

	C3D_UpdateUniforms(GPU_VERTEX_SHADER);
	C3D_UpdateUniforms(GPU_GEOMETRY_SHADER);
}

void C3Di_FinalizeFrame(u32** pBuf, u32* pSize)
{
	C3D_Context* ctx = C3Di_GetContext();

	if (ctx->flags & C3DiF_DrawUsed)
	{
		ctx->flags &= ~C3DiF_DrawUsed;
		GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);
		GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE, 1);
		GPUCMD_AddWrite(GPUREG_EARLYDEPTH_CLEAR, 1);
	}

	GPUCMD_Finalize();
	GPUCMD_GetBuffer(pBuf, NULL, pSize);
	ctx->cmdBufUsage = (float)(*pSize) / ctx->cmdBufSize;
	*pSize *= 4;

	ctx->flags ^= C3DiF_CmdBuffer;
	u32* buf = ctx->cmdBuf;
	if (ctx->flags & C3DiF_CmdBuffer)
		buf += ctx->cmdBufSize;
	GPUCMD_SetBuffer(buf, ctx->cmdBufSize, 0);
}

void C3D_FlushAsync(void)
{
	if (!(C3Di_GetContext()->flags & C3DiF_Active))
		return;

	u32* cmdBuf;
	u32 cmdBufSize;
	C3Di_FinalizeFrame(&cmdBuf, &cmdBufSize);

	//take advantage of GX_FlushCacheRegions to flush gsp heap
	extern u32 __ctru_linear_heap;
	extern u32 __ctru_linear_heap_size;
	GX_FlushCacheRegions(cmdBuf, cmdBufSize, (u32 *) __ctru_linear_heap, __ctru_linear_heap_size, NULL, 0);
	GX_ProcessCommandList(cmdBuf, cmdBufSize, 0x0);
}

float C3D_GetCmdBufUsage(void)
{
	return C3Di_GetContext()->cmdBufUsage;
}

void C3D_Fini(void)
{
	C3D_Context* ctx = C3Di_GetContext();

	if (!(ctx->flags & C3DiF_Active))
		return;

	if (ctx->renderQueueExit)
		ctx->renderQueueExit();

	aptUnhook(&hookCookie);
	linearFree(ctx->cmdBuf);
	ctx->flags = 0;
}

void C3D_BindProgram(shaderProgram_s* program)
{
	C3D_Context* ctx = C3Di_GetContext();

	if (!(ctx->flags & C3DiF_Active))
		return;

	shaderProgram_s* oldProg = ctx->program;
	shaderInstance_s* newGsh = program->geometryShader;
	if (oldProg != program)
	{
		ctx->program = program;
		ctx->flags |= C3DiF_Program;

		if (oldProg)
		{
			if (oldProg->vertexShader->dvle->dvlp != program->vertexShader->dvle->dvlp)
				ctx->flags |= C3DiF_VshCode;
			shaderInstance_s* oldGsh = oldProg->geometryShader;
			if (newGsh && (!oldGsh || oldGsh->dvle->dvlp != newGsh->dvle->dvlp))
				ctx->flags |= C3DiF_GshCode;
		} else
			ctx->flags |= C3DiF_VshCode | C3DiF_GshCode;
	}

	C3Di_LoadShaderUniforms(program->vertexShader);
	if (newGsh)
		C3Di_LoadShaderUniforms(newGsh);
	else
		C3Di_ClearShaderUniforms(GPU_GEOMETRY_SHADER);
}

C3D_FVec* C3D_FixedAttribGetWritePtr(int id)
{
	if (id < 0 || id >= 12)
		return NULL;

	C3D_Context* ctx = C3Di_GetContext();

	if (!(ctx->flags & C3DiF_Active))
		return NULL;

	ctx->fixedAttribDirty     |= BIT(id);
	ctx->fixedAttribEverDirty |= BIT(id);
	return &ctx->fixedAttribs[id];
}
