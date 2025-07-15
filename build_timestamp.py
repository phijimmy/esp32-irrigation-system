# PlatformIO extra script to inject build time as a macro
import datetime
import os

def before_build(source, target, env):
    now = datetime.datetime.now()
    build_time = now.strftime("%Y-%m-%d %H:%M:%S")
    env.Append(CPPDEFINES=[('BUILD_TIME', '"{}"'.format(build_time))])
