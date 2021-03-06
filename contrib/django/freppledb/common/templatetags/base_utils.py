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

from decimal import Decimal

from django.template import Library, Node, Variable, TemplateSyntaxError
from django.template.loader import get_template
from django.conf import settings
from django.contrib.admin.models import LogEntry
from django.utils.translation import ugettext as _
from django.utils.http import urlquote
from django.utils.encoding import iri_to_uri, force_unicode
from django.utils.html import escape
from django.db import DEFAULT_DB_ALIAS

from freppledb.execute.models import Scenario
from freppledb import VERSION


HOME_CRUMB = '<a href="%s/admin/">%s</a>'
NUMBER_OF_CRUMBS = 5

register = Library()
variable_title = Variable("title")
variable_request = Variable("request")
variable_popup = Variable("is_popup")

#
# A tag to find all models a user is allowed to see
#

class ModelsNode(Node):
    def __init__(self, adminsite, varname):
        self.varname = varname
        dot = adminsite.rindex('.')
        self.adminsite = getattr(__import__(adminsite[:dot], {}, {}, ['']), adminsite[dot+1:])

    def render(self, context):
        from django.utils.text import capfirst
        user = context['user']
        model_list = []
        for m in self.adminsite._registry:
          # Verify if the model is allowed to be displayed in the admin ui and
          # check the user has appropriate permissions to access it
          if user.has_perm("%s.%s" % (m._meta.app_label, m._meta.get_change_permission())):
            model_list.append({
                 'name': capfirst(m._meta.verbose_name_plural),
                 'verbose_name': capfirst(m._meta.verbose_name),
                 'admin_url': '/%s/%s/%s/' % (self.adminsite.name, m._meta.app_label, m.__name__.lower()),
                 'can_add': user.has_perm("%s.%s" % (m._meta.app_label, m._meta.get_add_permission()))
                 })
        model_list.sort(key = lambda m : m['verbose_name'])
        context[self.varname] = model_list
        return ''


def get_models(parser, token):
    """
    Returns a list of output models to which the user has permissions.
    Syntax::
        {% get_models from [admin_site] as [context_var_containing_app_list] %}
    """
    tokens = token.contents.split()
    if len(tokens) < 5:
        raise TemplateSyntaxError("'%s' tag requires 5 arguments" % tokens[0])
    if tokens[1] != 'from':
        raise TemplateSyntaxError("First argument to '%s' tag must be 'from'" % tokens[0])
    if tokens[3] != 'as':
        raise TemplateSyntaxError("Third argument to '%s' tag must be 'as'" % tokens[0])
    return ModelsNode(tokens[2],tokens[4])

register.tag('get_models', get_models)


#
# A tag to create breadcrumbs on your site
#

class CrumbsNode(Node):
  r'''
  A generic breadcrumbs framework.

  Usage in your templates:
  {% crumbs %}

  The admin app already defines a block for crumbs, so the typical usage of the
  crumbs tag is as follows:
  {%block breadcrumbs%}<div class="breadcrumbs">{%crumbs%}</div>{%endblock%}
  '''
  def render(self, context):
    try: req = context['request']
    except: return ''  # No request found in the context: no crumbs...
    if not hasattr(req,'session'): return # No session found in the context: no crumbs...

    # Pick up the current crumbs from the session cookie
    try:
      cur = req.session['crumbs']
      try: cur = cur[req.prefix]
      except: cur = [(unicode(_('Home')), HOME_CRUMB % (req.prefix, _('Home')), '%s/admin/' % req.prefix)]
    except:
      req.session['crumbs'] = {}
      cur = [(unicode(_('Home')), HOME_CRUMB % (req.prefix, _('Home')), '%s/admin/' % req.prefix)]

    # Compute the new crumb node
    try: title = variable_title.resolve(context)
    except: title = req.get_full_path()
    # A special case to work around the hardcoded title of the main admin page
    if title == _('Site administration'): title = _('Home')
    node = (unicode(title),
      '<a href="%s%s%s">%s</a>' % (
        req.prefix, urlquote(req.path),
        req.GET and ('?' + iri_to_uri(req.GET.urlencode())) or '',
        unicode(escape(title))
        ),
      '%s%s%s' % (
        req.prefix, urlquote(req.path),
        req.GET and ('?' + iri_to_uri(req.GET.urlencode())) or '',
        ),
      )

    # Pop from the stack if the same title is already in the crumbs.
    cnt = 0
    for i in cur:
      if i[0] == node[0]:
        cur = cur[0:cnt]   # Pop all remaining elements from the stack
        break
      cnt += 1

    # Keep only a limited number of links in the history.
    # We delete the second element to keep "home" at the head of the list.
    while len(cur) > NUMBER_OF_CRUMBS: del cur[1]

    # Push current URL on the stack
    cur.append( node )

    # Update the current session
    req.session['crumbs'][req.prefix] = cur

    # Now create HTML code to return
    return '  >  '.join([i[1] for i in cur])

  def __repr__(self):
    return "<Crumbs Node>"

def do_crumbs(parser, token):
    return CrumbsNode()

register.tag('crumbs', do_crumbs)


#
# A tag to update a context variable
#

class SetVariable(Node):
  def __init__(self, varname, value):
    self.varname = varname
    self.value = value

  def render(self, context):
    var = Variable(self.value).resolve(context)
    if var:
      context[self.varname] = var
    else:
      context[self.varname] = context[self.value]
    return ''

  def __repr__(self):
    return "<SetVariable Node>"


def set_var(parser, token):
  r'''
  Example:
  {% set category_list category.categories.all %}
  {% set dir_url "../" %}
  {% set type_list "table" %}
  '''
  from re import split
  bits = split(r'\s+', token.contents, 2)
  if len(bits) < 2:
      raise TemplateSyntaxError("'%s' tag requires two arguments" % bits[0])
  return SetVariable(bits[1],bits[2])

register.tag('set', set_var)



#
# A tag to include the tabs for a model
#

class ModelTabs(Node):
  def __init__(self, model):
    self.model = model

  def render(self, context):
    try:
      model = Variable(self.model).resolve(context)
      template = get_template("%stabs.html" % model)
      return template.render(context)
    except:
      if settings.TEMPLATE_DEBUG: raise
      return ''


def get_modeltabs(parser, token):
  r'''
  {% tabs "customer" %}
  {% tabs myvariable %}
  '''
  from re import split
  bits = split(r'\s+', token.contents, 1)
  if len(bits) != 2:
    raise TemplateSyntaxError("'%s' tag requires 1 argument" % bits[0])
  return ModelTabs(bits[1])

register.tag('tabs', get_modeltabs)



#
# A tag to return HTML code for a database selector
#

class SelectDatabaseNode(Node):
  r'''
  A tag to return HTML code for a database selector.
  '''
  def render(self, context):
    try: req = context['request']
    except: return ''  # No request found in the context
    scenarios = Scenario.objects.filter(status=u'In use').values('name')
    if len(scenarios) <= 1: return ''
    s = [u'<form>%s&nbsp;<select id="database" name="%s" onchange="selectDatabase()">' % (force_unicode(_("Model:")), req.database) ]
    for i in scenarios:
      i = i['name']
      if i == req.database:
        s.append(u'<option value="%s" selected="selected">%s</option>' % (i,i))
      else:
        s.append(u'<option value="%s">%s</option>' % (i,i))
    s.append(u'</select></form>')
    return ''.join(s)

  def __repr__(self):
    return "<SelectDatabase Node>"

def selectDatabase(parser, token):
    return SelectDatabaseNode()

register.tag('selectDatabase', selectDatabase)


#
# A tag to return the most recent actions of a user
#
# This code is a slightly modified version of a standard Django tag.
# The only change is to look for the logentry records in the right database.
# See the file django\contrib\admin\templatetags\log.py
#

class MultiDBAdminLogNode(Node):
  def __init__(self, limit, varname, user):
    self.limit, self.varname, self.user = limit, varname, user

  def render(self, context):
    try: db = context['request'].database or DEFAULT_DB_ALIAS
    except: db = DEFAULT_DB_ALIAS
    if self.user is None:
      context[self.varname] = LogEntry.objects.using(db).select_related('content_type', 'user')[:self.limit]
    else:
      user_id = self.user
      if not user_id.isdigit():
          user_id = context[self.user].id
      context[self.varname] = LogEntry.objects.using(db).filter(user__id__exact=user_id).select_related('content_type', 'user')[:self.limit]
    return ''


class DoGetMultiDBAdminLog:
  """
  Populates a template variable with the admin log for the given criteria.

  Usage::

      {% get_admin_log [limit] as [varname] for_user [context_var_containing_user_obj] %}

  Examples::

      {% get_admin_log 10 as admin_log for_user 23 %}
      {% get_admin_log 10 as admin_log for_user user %}
      {% get_admin_log 10 as admin_log %}

  Note that ``context_var_containing_user_obj`` can be a hard-coded integer
  (user ID) or the name of a template context variable containing the user
  object whose ID you want.
  """
  def __init__(self, tag_name):
    self.tag_name = tag_name

  def __repr__(self):
    return "<GetMultiDBAdminLog Node>"

  def __call__(self, parser, token):
    tokens = token.contents.split()
    if len(tokens) < 4:
      raise TemplateSyntaxError("'%s' statements require two arguments" % self.tag_name)
    if not tokens[1].isdigit():
      raise TemplateSyntaxError("First argument in '%s' must be an integer" % self.tag_name)
    if tokens[2] != 'as':
      raise TemplateSyntaxError("Second argument in '%s' must be 'as'" % self.tag_name)
    if len(tokens) > 4:
      if tokens[4] != 'for_user':
        raise TemplateSyntaxError("Fourth argument in '%s' must be 'for_user'" % self.tag_name)
    return MultiDBAdminLogNode(limit=tokens[1], varname=tokens[3], user=(len(tokens) > 5 and tokens[5] or None))

register.tag('get_multidbadmin_log', DoGetMultiDBAdminLog('get_admin_log'))


#
# A simple tag returning the frePPLe version
#

@register.simple_tag
def version():
  '''
  A simple tag returning the version of the frePPLe application.
  '''
  return VERSION

version.is_safe = True


#
# A filter to format a duration
#

def duration(value):
  try:
    if value == None: return ''
    value = Decimal(force_unicode(value))
    if value == 0: return '0 s'
    if value % 604800 == 0: return '%.2f w' % (value/Decimal('604800.0'))
    if value % 3600 != 0 and value < 86400: return '%.2f s' % value
    if value % 86400 != 0 and value < 604800: return '%.2f h' % (value/Decimal('3600'))
    return '%.2f d' % (value/Decimal('86400'))
  except Exception:
    return ''

duration.is_safe = True
register.filter('duration', duration)


#
# Filters to get the verbose name of a model
#

def verbose_name(obj):
  return obj._meta.verbose_name
register.filter(verbose_name)

def verbose_name_plural(obj):
  return obj._meta.verbose_name_plural
register.filter(verbose_name_plural)

def app_label(obj):
  return obj._meta.app_label
register.filter(app_label)

def object_name(obj):
  return obj._meta.object_name
register.filter(object_name)
