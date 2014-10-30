# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
VERSION='0.1'
APPNAME="ndn-traffic-generator"

def options(opt):
    opt.load(['compiler_cxx', 'gnu_dirs'])
    opt.load(['default-compiler-flags'], tooldir=['.waf-tools'])

def configure(conf):
    conf.load(['compiler_cxx', 'gnu_dirs',
               'default-compiler-flags'])
    conf.check_cfg(package='libndn-cxx', args=['--cflags', '--libs'],
                   uselib_store='NDN_CXX', mandatory=True)

def build(bld):
    bld.program(
        features='cxx',
        target='ndn-traffic',
        source='src/ndn-traffic-client.cpp',
        use='NDN_CXX',
        )

    bld.program(
        features='cxx',
        target='ndn-traffic-server',
        source='src/ndn-traffic-server.cpp',
        use='NDN_CXX',
        )

    bld.install_files('${SYSCONFDIR}/ndn', ['ndn-traffic-client.conf.sample',
                                            'ndn-traffic-server.conf.sample'])
