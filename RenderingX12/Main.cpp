//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "RenderingX.h"
#include "resource.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	RenderingX renderingX(1280, 800, L"XUSG Scene Rendering");

	const auto hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RENDERINGX));

	return Win32Application::Run(&renderingX, hInstance, nCmdShow, hIcon);
}
