#===------------------------------------------------------------------------===#
#
# NAME         : serve
# PURPOSE      : Runs a web server that exposes the API of the gitjson program.
# COPYRIGHT    : (c) 2014 Sean Donnellan. All Rights Reserved.
# LICENSE      : The MIT License (see LICENSE.txt for details)
# DESCRIPTION  : Hosts a HTTP server on port 7723 and spawns the gitjson program
#                to respond to the queries.
#
#===------------------------------------------------------------------------===#
from __future__ import print_function

import os
import subprocess
import sys
import tempfile
try:
  from BaseHTTPServer import HTTPServer
  from SimpleHTTPServer import SimpleHTTPRequestHandler
except ModuleNotFoundError:
  from http.server import BaseHTTPRequestHandler as SimpleHTTPRequestHandler
  from http.server import HTTPServer


class Forwarder(SimpleHTTPRequestHandler):
  """Forward the information on to another process."""

  def __init__(self,req,client_addr,server):
    SimpleHTTPRequestHandler.__init__(self, req, client_addr, server)

  def do_GET(self):
    # Execute the program.
    if '/file/' in self.path:
      i = self.path.rfind('?')
      if i > 0:
        k, v = self.path[i+1:].split('=')
        queries = {k: v}
        # NOTE: The gitjson forwarder doesn't handle the query string part
        # at the moment, so strip it off instead of just passing it through.
        self.path = self.path[:i]
      else:
        queries = {}

    stdout, stderr = self.execute(self.path)
    response = stderr if stderr else stdout

    if isinstance(response, tuple):
      # This handles if reponse is made up of (lines, content length).
      response, contentLength = response
    else:
      contentLength = len(response)


    self.send_response(500 if stderr else 200)
    if '/file/' in self.path:
      self.send_header("Content-type", "application/octet-stream")
      filename = queries.get('filename')
      if filename:
        self.send_header("Content-disposition", "attachment;filename=\"%s\"" %
                         filename)
    else:
      self.send_header("Content-type", "application/json; charset=utf-8")

    self.send_header("Content-length", contentLength)
    self.end_headers()
    if isinstance(response, list):
      for element in response:
        self.wfile.write(element)
    else:
      self.wfile.write(response)

class GitRunner(Forwarder):
  """
  Spawn a process running gitjson that stays arounds.

  The intention is to elimiate the overhead of having to start a process and
  set-up gitjson each time a request is made.
  """

  gitjsonexe = r'build\Release_x64\gitjson.exe'

  gitjsonprocess = None

  def execute(self, path):
    """Executes the git json executable and returns the results."""

    # Spawn the process if it wasn't started already.
    process = self.process()

    # Send the command
    process.stdin.write(path + '\n')

    # Read the output
    length = 0
    lines = []
    line = process.stdout.readline()
    while line and not line.startswith(chr(4)):
      lines.append(line)
      length += len(line)
      line = process.stdout.readline()

    return (lines, length), ''

  def process(self):
    """
    Returns the handle to the gitjson process.

    If the process wasn't spawned (opened) yet then it opens it.
    """
    if not GitRunner.gitjsonprocess:
      env = {
        'BASE_URI': 'http://%s:%d' % (
          self.server.server_name, self.server.server_port),
        }

      stderr = tempfile.TemporaryFile(mode="w+t")
      try:
        GitRunner.gitjsonprocess = subprocess.Popen(
          args=[self.gitjsonexe, '-'],
          executable=self.gitjsonexe,
          env=env,
          stdin=subprocess.PIPE,
          stdout=subprocess.PIPE,
          stderr=stderr,
          )

      except EnvironmentError as e:
        print('error: invoking process: ', e)
        return None

    return GitRunner.gitjsonprocess

class GitForwarder(Forwarder):
  """
  Spawns a process running gitjson each time a request is made and passes it
  the given URI on the command line and reads its output and forwards it onto
  the requester.
  """
  gitjsonexe = r'build\Release_x64\gitjson.exe'

  def execute(self, path):
    """Executes the git json executable and returns the results."""

    stdout = tempfile.TemporaryFile(mode="w+t")
    stderr = tempfile.TemporaryFile(mode="w+t")

    env = {
      'BASE_URI': 'http://%s:%d' % (
        self.server.server_name, self.server.server_port),
      }

    try:
      p = subprocess.Popen(
        args=[self.gitjsonexe, self.path],
        executable=self.gitjsonexe,
        env=env,
        stdin=subprocess.PIPE,
        stdout=stdout,
        stderr=stderr,
        )
    except EnvironmentError as e:
      print('error: invoking process: ', e)
      return None, None
    p.stdin.close()

    exitCode = p.wait()
    stdout.seek(0)
    stderr.seek(0)

    stdoutText = stdout.read()
    return stdoutText, stderr.read()

if __name__ == '__main__':
  if sys.argv[1:]:
      port = int(sys.argv[1])
  else:
      port = 7723
  server_address = ('', port)

  # At the moment there is very little benefit of reusing the process as all
  # it saves is the process creation and setting up of routes. The git repo
  # is closed after it response to each request so its in-memory caches etc
  # are cleared.
  reuseProcess = False
  if reuseProcess:
    httpd = HTTPServer(server_address, GitRunner)
  else:
    httpd = HTTPServer(server_address, GitForwarder)

  sa = httpd.socket.getsockname()
  print("Serving HTTP on", sa[0], "port", sa[1], "...")
  httpd.serve_forever()

#===---------------------------- End of the file ---------------------------===#
