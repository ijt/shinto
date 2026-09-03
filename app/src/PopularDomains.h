// Offline omnibox autocomplete: a baked-in list of the 100k most popular
// domains (app/resources/top100k-domains.txt -- see the .SOURCE.md next to
// it), not a live network request. Nothing the user types ever leaves the
// machine to power suggestions.
#pragma once

#include <QString>
#include <QVector>

namespace shinto {

class PopularDomains {
 public:
  struct Suggestion {
    QString label;  // the bare domain, e.g. "youtube.com"
    QString url;    // "https://" + label
  };

  // Loads and sorts the baked-in list once. Cheap enough (~100k short
  // strings) to do synchronously at daemon startup.
  PopularDomains();

  // Prefix match against the domain list (case-insensitive), ranked by
  // popularity (source-list order) among matches, not alphabetically. A
  // prefix shorter than 2 characters returns nothing -- consistent with
  // the old typed/history completion's threshold, and avoids single-letter
  // prefixes matching thousands of entries for no useful purpose.
  QVector<Suggestion> complete(const QString &prefix, int limit) const;

 private:
  struct Entry {
    QString domain;
    int rank;
  };
  QVector<Entry> domains_;  // sorted alphabetically by domain
};

}  // namespace shinto
