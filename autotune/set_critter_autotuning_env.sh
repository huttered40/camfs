#!/bin/bash

export CRITTER_TEST=space2
export CRITTER_MECHANISM=1
export CRITTER_AUTOTUNING_AGGREGATION_MODE=0
export CRITTER_AUTOTUNING_OPTIMIZE=1
export CRITTER_COMM_ENVELOPE_PARAM=0
export CRITTER_COMM_UNIT_PARAM=0
export CRITTER_COMM_ANALYSIS_PARAM=0
export CRITTER_COMP_ENVELOPE_PARAM=0
export CRITTER_COMP_UNIT_PARAM=0
export CRITTER_COMP_ANALYSIS_PARAM=0
export CRITTER_PATTERN_ERROR_LIMIT=0.1
export CRITTER_PATTERN_TIME_LIMIT=0.00001
export CRITTER_PATTERN_COUNT_LIMIT=3
#export CRITTER_UPDATE_ANALYSIS=1
#export CRITTER_TRACK_COLLECTIVE=1
#export CRITTER_TRACK_P2P=1
#export CRITTER_TRACK_BLAS=0
#export CRITTER_TRACK_LAPACK=0
#export CRITTER_SCHEDULE_KERNELS=1
export CRITTER_VIZ_FILE=${CRITTER_TEST}__mechanism-${CRITTER_MECHANISM}_mode-${CRITTER_AUTOTUNING_AGGREGATION_MODE}_opt-${CRITTER_AUTOTUNING_OPTIMIZE}_comm-envelope-${CRITTER_COMM_ENVELOPE_PARAM}_comm-unit-${CRITTER_COMM_UNIT_PARAM}_comm-analysis-${CRITTER_COMM_ANALYSIS_PARAM}_comp-envelope-${CRITTER_COMP_ENVELOPE_PARAM}_comp-unit-${CRITTER_COMP_UNIT_PARAM}_comp-analysis-${CRITTER_COMP_ANALYSIS_PARAM}_error-limit-${CRITTER_PATTERN_ERROR_LIMIT}_time-limit-${CRITTER_PATTERN_TIME_LIMIT}_count-limit-${CRITTER_PATTERN_COUNT_LIMIT}
