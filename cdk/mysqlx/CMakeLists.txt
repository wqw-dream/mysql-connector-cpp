# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0, as
# published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an
# additional permission to link the program and your derivative works
# with the separately licensed software that they have included with
# MySQL.
#
# Without limiting anything contained in the foregoing, this file,
# which is part of MySQL Connector/C++, is also subject to the
# Universal FOSS Exception, version 1.0, a copy of which can be found at
# http://oss.oracle.com/licenses/universal-foss-exception.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA


SET(target_mysqlx ${cdk_target_prefix}mysqlx CACHE INTERNAL "CDK mysqlx target")

ADD_DEFINITIONS(-DWIN32_LEAN_AND_MEAN -DNOGDI)
ADD_DEFINITIONS(-DNOMINMAX)
ADD_DEFINITIONS(-DSIZEOF_LONG=${SIZEOF_LONG} -DSIZEOF_LONG_LONG=${SIZEOF_LONG_LONG})

# TODO: Fix compile warnings in auth_mysql41.cc

if(WIN32)
  set_property(SOURCE auth_hash.cc
    PROPERTY COMPILE_FLAGS "/W3"
  )
endif()


ADD_LIBRARY(${target_mysqlx} OBJECT
  session.cc
  result.cc
  auth_hash.cc
  )
ADD_COVERAGE(${target_mysqlx})

# Note: File auth_mysql41.cc and auth_sha256.cc uses SSL code.

add_ssl(${target_mysqlx})

#TARGET_LINK_LIBRARIES(${target_mysqlx} ${target_proto_mysqlx})

ADD_SUBDIRECTORY(tests)
