#
# Copyright (C) 2007-2012 by Johan De Taeye, frePPLe bvba
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
# General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public
# License along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from django.contrib.auth.backends import ModelBackend
from django.core.validators import email_re

from freppledb.common.models import User


class EmailBackend(ModelBackend):
  '''
  This customized authentication allows logging in using either
  the user name or the user email address.
  '''
  def authenticate(self, username=None, password=None):
    if email_re.search(username):
      # The username looks like an email address
      try:
        user = User.objects.get(email=username)
        if user.check_password(password):
          return user
      except User.DoesNotExist:
        return None
    else:
      # The user name isn't an email address
      try:
        user = User.objects.get(username=username)
        if user.check_password(password):
          return user
      except User.DoesNotExist:
        return None
