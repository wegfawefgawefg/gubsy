#pragma once

struct EngineState;

void playing_process_inputs(EngineState& engine, void* app_context);
void playing_step(EngineState& engine, void* app_context);
void playing_draw(EngineState& engine, void* app_context);
