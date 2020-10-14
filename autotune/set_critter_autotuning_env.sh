#!/bin/bash

export MKL_NUM_THREADS=1

export CRITTER_MODE=1
export CRITTER_MECHANISM=1
export CRITTER_AUTOTUNING_SCHEDULE_TAG=cholinv
export CRITTER_AUTOTUNING_TEST=1

export CRITTER_AUTOTUNING_COMM_SAMPLE_AGGREGATION_MODE=0
export CRITTER_AUTOTUNING_COMM_STATE_AGGREGATION_MODE=2
export CRITTER_AUTOTUNING_COMP_SAMPLE_AGGREGATION_MODE=0
export CRITTER_AUTOTUNING_COMP_STATE_AGGREGATION_MODE=0

export CRITTER_COMM_ENVELOPE_PARAM=0
export CRITTER_COMM_UNIT_PARAM=0
export CRITTER_COMM_ANALYSIS_PARAM=0
export CRITTER_COMP_ENVELOPE_PARAM=0
export CRITTER_COMP_UNIT_PARAM=0
export CRITTER_COMP_ANALYSIS_PARAM=0

export CRITTER_AUTOTUNING_DELTA=2

export CRITTER_AUTOTUNING_SAMPLE_CONSTRAINT_MODE=0

export CRITTER_PATTERN_ERROR_LIMIT=0.0
export CRITTER_VIZ_FILE=Test-${CRITTER_AUTOTUNING_TEST}__mechanism-${CRITTER_MECHANISM}_comm-sample-agg-mode-${CRITTER_AUTOTUNING_COMM_SAMPLE_AGGREGATION_MODE}_comm-sample-state-agg-mode-${CRITTER_AUTOTUNING_COMM_STATE_AGGREGATION_MODE}_comp-sample-agg-mode-${CRITTER_AUTOTUNING_COMP_SAMPLE_AGGREGATION_MODE}_comp-sample-state-agg-mode-${CRITTER_AUTOTUNING_COMP_STATE_AGGREGATION_MODE}_sample-constraint-mode-${CRITTER_AUTOTUNING_SAMPLE_CONSTRAINT_MODE}_delta-${CRITTER_AUTOTUNING_DELTA}_error-limit-${CRITTER_PATTERN_ERROR_LIMIT}
