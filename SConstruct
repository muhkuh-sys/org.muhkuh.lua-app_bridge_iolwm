# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------- #
#   Copyright (C) 2021 by Christoph Thelen                                #
#   doc_bacardi@users.sourceforge.net                                     #
#                                                                         #
#   This program is free software; you can redistribute it and/or modify  #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 2 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   This program is distributed in the hope that it will be useful,       #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program; if not, write to the                         #
#   Free Software Foundation, Inc.,                                       #
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             #
# ----------------------------------------------------------------------- #

import os.path


# ---------------------------------------------------------------------------
#
# Set up the Muhkuh Build System.
#
SConscript('mbs/SConscript')
Import('atEnv')


# ---------------------------------------------------------------------------
#
# Create the compiler environments.
#

# Create a build environment for the Cortex-M4 based netX chips.
env_cortexM4 = atEnv.DEFAULT.CreateEnvironment(['gcc-arm-none-eabi-4.9', 'asciidoc'])
env_cortexM4.CreateCompilerEnv('NETX90_APP', ['arch=armv7', 'thumb'], ['arch=armv7e-m', 'thumb'])


# ---------------------------------------------------------------------------
#
# Build the platform library.
#
SConscript('platform/SConscript')


# ----------------------------------------------------------------------------
#
# Get the source code version from the VCS.
#
atEnv.DEFAULT.Version('targets/version/version.h', 'templates/version.h')


# ----------------------------------------------------------------------------
#
# Build the app bridge module.
#
SConscript('module/SConscript')
Import('BRIDGE_IOLWM_BIN', 'BRIDGE_IOLWM_LUA')

"""
# ----------------------------------------------------------------------------
#
# Build the documentation.
#

# Get the default attributes.
aAttribs = atEnv.DEFAULT['ASCIIDOC_ATTRIBUTES']
# Add some custom attributes.
aAttribs.update(dict({
    # Use ASCIIMath formulas.
    'asciimath': True,

    # Embed images into the HTML file as data URIs.
    'data-uri': True,

    # Use icons instead of text for markers and callouts.
    'icons': True,

    # Use numbers in the table of contents.
    'numbered': True,

    # Generate a scrollable table of contents on the left of the text.
    'toc2': True,

    # Use 4 levels in the table of contents.
    'toclevels': 4
}))
tDoc = atEnv.DEFAULT.Asciidoc(
    'targets/doc/org.muhkuh.tests-iomatrix.html',
    'doc/org.muhkuh.tests-iomatrix.asciidoc',
    ASCIIDOC_BACKEND='html5',
    ASCIIDOC_ATTRIBUTES=aAttribs
)
"""


# ---------------------------------------------------------------------------
#
# Build an archive.
#
strGroup = PROJECT_GROUP
strModule = PROJECT_MODULE

# Split the group by dots.
aGroup = strGroup.split('.')
# Build the path for all artifacts.
strModulePath = 'targets/jonchki/repository/%s/%s/%s' % ('/'.join(aGroup), strModule, PROJECT_VERSION)

strArtifact = 'app_bridge_iolwm'

tArcList = atEnv.DEFAULT.ArchiveList('zip')

#tArcList.AddFiles('doc/',
#                  doc)

tArcList.AddFiles('netx/',
                  BRIDGE_IOLWM_BIN)

tArcList.AddFiles('lua/app_bridge/modules/',
                  BRIDGE_IOLWM_LUA)

tArcList.AddFiles('',
                  'installer/%s-%s/install.lua' % (strGroup, strModule))


strBasePath = os.path.join(strModulePath, '%s-%s' % (strArtifact, PROJECT_VERSION))
tArtifact = atEnv.DEFAULT.Archive('%s.zip' % strBasePath, None, ARCHIVE_CONTENTS = tArcList)
tArtifactHash = atEnv.DEFAULT.Hash('%s.hash' % tArtifact[0].get_path(), tArtifact[0].get_path(), HASH_ALGORITHM='md5,sha1,sha224,sha256,sha384,sha512', HASH_TEMPLATE='${ID_UC}:${HASH}\n')
tConfiguration = atEnv.DEFAULT.Version('%s.xml' % strBasePath, 'installer/%s-%s/%s.xml' % (strGroup, strModule, strArtifact))
tConfigurationHash = atEnv.DEFAULT.Hash('%s.hash' % tConfiguration[0].get_path(), tConfiguration[0].get_path(), HASH_ALGORITHM='md5,sha1,sha224,sha256,sha384,sha512', HASH_TEMPLATE='${ID_UC}:${HASH}\n')
tPom = atEnv.DEFAULT.ArtifactVersion('%s.pom' % strBasePath, 'installer/%s-%s/%s.pom' % (strGroup, strModule, strArtifact))
