# Output of the safety supervisor.
uint8 NOMINAL=0
uint8 DEGRADED=1
uint8 SAFE_STOP=2

std_msgs/Header header
uint8 state
float32 speed_limit_mps          # m/s, 0 when SAFE_STOP
uint32 reasons                   # bit flags, see guardrail_core/supervisor.hpp ReasonFlag
