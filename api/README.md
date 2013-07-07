
License:
  Under the MIT license, see LICENSE.txt for details.

Features:

* Host a HTTP server
* Generate a html pages for each commit (logs etc).
* (Optionally) generate "JSON" or some interface so a purely "single page" could
   be made

Pages:

  / - Main screen
  /<repo-name> - Home screen for a given repo
  /<repo-name>/branches - List the branches in that repo
  /<repo-name>/tags - List the tags in that repo
  /<repo-name/commit/<hash> - Information for that hash.

  /<repo-name> - Home screen for a given repo

JSON interface:
  /api/repos/<repo-name> - Summary of that repo.
  /api/repos/<repo-name>/branches - List the branches in that repo
  /api/repos/<repo-name>/tags - List the tags in that repo
  /api/repos/<repo-name>/tags/<name> - Information about that tag.
  /api/repos/<repo-name/commit/<hash> - Information for that hash.
