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
import os
import subprocess
import sys
import tempfile
import BaseHTTPServer
from SimpleHTTPServer import SimpleHTTPRequestHandler

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

    self.send_response(500 if stderr else 200)
    if '/file/' in self.path:
      self.send_header("Content-type", "application/octet-stream")
      filename = queries.get('filename')
      if filename:
        self.send_header("Content-disposition", "attachment;filename=\"%s\"" %
                         filename)
    else:
      self.send_header("Content-type", "application/json; charset=utf-8")
    self.send_header("Content-length", len(response))
    self.end_headers()
    self.wfile.write(response)


class GitForwarder(Forwarder):
  gitjsonexe = r'build\Debug\bin\gitjson.exe'

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
    except EnvironmentError, e:
      print 'error: invoking process: ', e
      return None
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

  httpd = BaseHTTPServer.HTTPServer(server_address, GitForwarder)

  sa = httpd.socket.getsockname()
  print "Serving HTTP on", sa[0], "port", sa[1], "..."
  httpd.serve_forever()

#===---------------------------- End of the file ---------------------------===#
