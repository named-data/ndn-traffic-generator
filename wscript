# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
VERSION='0.1'
APPNAME="ndn-traffic-generator"

from waflib import Utils
import os

def options(opt):
    opt.load(['compiler_cxx', 'gnu_dirs'])
    opt.load(['default-compiler-flags'], tooldir=['.waf-tools'])

def configure(conf):
    conf.load(['compiler_cxx', 'gnu_dirs',
               'default-compiler-flags'])

    if 'PKG_CONFIG_PATH' not in os.environ:
        os.environ['PKG_CONFIG_PATH'] = Utils.subst_vars('${LIBDIR}/pkgconfig', conf.env)
    conf.check_cfg(package='libndn-cxx', args=['--cflags', '--libs'],
                   uselib_store='NDN_CXX', mandatory=True)

def build(bld):
    bld.program(
        features='cxx',
        target='ndn-consumer',
        source='src/ndn-consumer.cpp',
        use='NDN_CXX',
        )

    bld.program(
        features='cxx',
        target='ndn-producer',
        source='src/ndn-producer.cpp',
        use='NDN_CXX',
        )

    bld.program(
        features='cxx',
        target='ndn-traffic-server',
        source='src/ndn-traffic-server.cpp',
        use='NDN_CXX',
        )
