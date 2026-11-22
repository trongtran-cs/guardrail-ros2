# Perception health summary published by the detector node.
# Units: SI. Frame: camera optical frame unless stated (REP-103).
std_msgs/Header header
float32 confidence               # [0,1] detector confidence
float32 nearest_person_m         # metres; negative = no person detected
uint64 pipeline_latency_ns       # nanoseconds, capture -> detection published
