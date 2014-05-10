#===------------------------------------------------------------------------===#
#
# NAME         : apitest
# PURPOSE      : Tests the API exposed by the gitjson program.
# COPYRIGHT    : (c) 2014 Sean Donnellan. All Rights Reserved.
# LICENSE      : The MIT License (see LICENSE.txt for details)
# DESCRIPTION  : Connects to a server that exposes the gitjson API and makes
#                requests to check the apporiate response.
#
# The git repository for the "git" project is used as the subject of the tests.
#
# Before running this script run server.py which starts a web server on port
# 7723 which this script then connects to.
#
#===------------------------------------------------------------------------===#

# The goal of this script is to test walking the tree.
import requests

import json
import unittest

class ApiTester(unittest.TestCase):
  """
  Tests various parts of the API.
  """

  def setUp(self):
    self.baseUri = 'http://localhost:7723/api/repos/git'
    self.jsonContentType = 'application/json; charset=utf-8'

  def test_repository_information(self):
    """Tests getting the general information about a repository.

    It should contain the name, the list of branches and the tags.
    """
    r = requests.get(self.baseUri)
    self.assertEqual(r.status_code, 200)
    self.assertEqual(r.headers['content-type'], self.jsonContentType)
    self.assertEqual(r.encoding, 'utf-8')

    response = r.json()

    self.assertEqual(response['repository'], 'git')
    self.assertIn('tags', response)
    self.assertIn('branches', response)

    self.assertIsInstance(response['tags'], list)
    self.assertIsInstance(response['branches'], list)

    self.assertIn('master', response['branches'])

  def test_branches(self):
    """Tests getting the list of branches in a repository."""
    r = requests.get(self.baseUri + '/branches')
    self.assertEqual(r.status_code, 200)
    self.assertEqual(r.headers['content-type'], self.jsonContentType)
    self.assertEqual(r.encoding, 'utf-8')

    response = r.json()

    self.assertEqual(response['repository'], 'git')
    self.assertIn('branches', response)

    self.assertIsInstance(response['branches'], list)

    self.assertIn('master', response['branches'])

  def test_tags(self):
    """Tests getting the list of tags in a repository."""
    r = requests.get(self.baseUri + '/tags')
    self.assertEqual(r.status_code, 200)
    self.assertEqual(r.headers['content-type'], self.jsonContentType)
    self.assertEqual(r.encoding, 'utf-8')

    response = r.json()

    self.assertEqual(response['repository'], 'git')
    self.assertIn('tags', response)

    self.assertIsInstance(response['tags'], list)

    tag = response['tags'][0]

    self.assertIsInstance(tag, dict)

    # A Tag should have a URI to the tag, the hash of the tag and the name.
    self.assertIn('url', tag)
    self.assertIn('hash', tag)
    self.assertIn('name', tag)

  def test_tag(self):
    """Tests getting a single in a repository."""
    r = requests.get(self.baseUri +
                     '/tags/683c35c9ade6e241df832a2f82c521264f6a9b7f')
    self.assertEqual(r.status_code, 200)
    self.assertEqual(r.headers['content-type'], self.jsonContentType)
    self.assertEqual(r.encoding, 'utf-8')

    response = r.json()

    self.assertIn('tag', response)
    self.assertIn('sha', response)
    self.assertEqual(response['sha'],
                     '683c35c9ade6e241df832a2f82c521264f6a9b7f')

    self.assertIn('url', response)
    self.assertTrue(response['url'].endswith(
        '/tags/683c35c9ade6e241df832a2f82c521264f6a9b7f'))

    self.assertIn('message', response)
    self.assertIn('tagger', response)
    self.assertIn('object', response)

    self.assertIsInstance(response['tagger'], dict)
    self.assertIsInstance(response['object'], dict)

    tagger = response['tagger']
    self.assertIn('name', tagger)
    self.assertIn('email', tagger)
    self.assertIn('date', tagger)

    self.assertEqual(tagger['name'], 'Junio C Hamano')
    self.assertEqual(tagger['email'], 'gitster@pobox.com')
    self.assertEqual(tagger['date'], '2010-10-22T00:14:53Z')

    objectValue = response['object']
    self.assertIn('type', objectValue)
    self.assertIn('sha', objectValue)
    self.assertIn('url', objectValue)

    self.assertEqual(objectValue['type'], 'commit')
    self.assertEqual(objectValue['sha'],
                     '8a90438506d3b7c8ef8bd802b7ed10c1f12da1d0')
    self.assertTrue(objectValue['url'].endswith(
        'commits/8a90438506d3b7c8ef8bd802b7ed10c1f12da1d0'))


  def test_refs(self):
    """Tests getting the list of references in a repository."""
    r = requests.get(self.baseUri + '/refs')
    self.assertEqual(r.status_code, 200)
    self.assertEqual(r.headers['content-type'], self.jsonContentType)
    self.assertEqual(r.encoding, 'utf-8')

    response = r.json()

    # This follows the GitHub API, so is just a list and doesn't contain the
    # name of the repository it is for.
    # See: https://developer.github.com/v3/git/refs/#get-all-references
    self.assertIsInstance(response, list)

    ref = response[0]

    self.assertIsInstance(ref, dict)

    # A Tag should have a URI to the tag, the hash of the tag and the name.
    self.assertIn('ref', ref)
    self.assertIn('url', ref)
    self.assertIn('object', ref)

    # The object should have at least these three properties.
    self.assertIn('url', ref['object'])
    self.assertIn('type', ref['object'])
    self.assertIn('sha', ref['object'])

  def test_refence(self):
    """Tests getting a single reference in a repository."""
    r = requests.get(self.baseUri + '/refs/heads/master')
    self.assertEqual(r.status_code, 200)
    self.assertEqual(r.headers['content-type'], self.jsonContentType)
    self.assertEqual(r.encoding, 'utf-8')

    ref = r.json()

    # This follows the GitHub API, so is just a list and doesn't contain the
    # name of the repository it is for.
    # See https://developer.github.com/v3/git/refs/#get-a-reference
    self.assertIsInstance(ref, dict)

    # A Tag should have a URI to the tag, the hash of the tag and the name.
    self.assertIn('ref', ref)
    self.assertEqual(ref['ref'], 'refs/heads/master')
    self.assertIn('url', ref)

    # This does not use equality because the address returned can be different
    # to the address requested (i.e localhost v.s computer for example).
    self.assertTrue(ref['url'].endswith('/refs/heads/master'))
    self.assertIn('object', ref)

    # The object should have at least these three properties.
    self.assertIn('url', ref['object'])
    self.assertIn('type', ref['object'])
    self.assertIn('sha', ref['object'])


class ServiceWalker(unittest.TestCase):
  """
  Tests walking down the links provided by the web service.

  At the moment this only tests that they give a 200 response and doesn't test
  that there is actually data there.
  """

  def setUp(self):
    self.baseUri = 'http://localhost:7723/api/repos/git'
    self.jsonContentType = 'application/json; charset=utf-8'

  @unittest.skip("this is not ready yet - it doesn't pass and it is incomplete")
  def test_links(self):
    """Teses getting all links from a set of URIs and following it all the way
    down.

    This test may take a while.
    """
    url = self.baseUri + '/refs'
    urlVisited = set(url)

    r = requests.get(url)
    self.assertEqual(r.status_code, 200)

    response = r.json()

    def findUrls(item):
      """
      Yields the values of all the "url" properties on any dict inside item.
      """
      if isinstance(item, dict):
        if 'url' in item:
          yield item['url']
        item = item.itervalues()
      elif not isinstance(item, list):
        return

      for value in item:
        for url in findUrls(value):
          yield url

    def visit(url):
      """Visit a given URL and collects the URLs in it."""
      r = requests.get(url)
      self.assertEqual(r.status_code, 200)
      self.assertEqual(r.headers['content-type'], self.jsonContentType)
      self.assertEqual(r.encoding, 'utf-8')

      try:
        data = r.json()
      except json.JSONDecodeError, e:
        self.fail('%s was not JSON' % url)

      return findUrls(data)

    urlsToVisit = list(findUrls(response))
    count = 0
    # Visit all the URLs
    while urlsToVisit:
      url = urlsToVisit.pop()
      if url in urlVisited:
        continue
      urlVisited.add(url)

      newUrls = visit(url)

      # Add the unvisited new URLs to the list.
      urlsToVisit.extend(u for u in newUrls if u not in urlVisited)

      count += 1
      if count % 43 == 0:
        print 'Entries left: %d' % len(urlsToVisit)


if __name__ == '__main__':
  unittest.main()

#===---------------------------- End of the file ---------------------------===#
