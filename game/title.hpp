#pragma once

struct EngineState;

void title_step(EngineState& engine, void* app_context);
void title_process_inputs(EngineState& engine, void* app_context);
void title_draw(EngineState& engine, void* app_context);
