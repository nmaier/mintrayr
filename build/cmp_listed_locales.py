import re
import logging

from path import path
from Mozilla.CompareLocales import compareDirs

logging.basicConfig()
logging.getLogger().setLevel(logging.WARNING)

def cmp_listed_locales(dirname):
    basedir = dirname if isinstance(dirname, path) else path(dirname)
    listed = dict()
    def parseCM():
        r = re.compile(r'^locale\s+.+?\s+(.+?)\s+(?:jar:(.+?)!/)?(.+?)\s*$')
        with open(basedir / "chrome.manifest", 'r') as fp:
            for l in fp:
                m = r.match(l)
                if not m:
                    continue
                l, j, p = m.groups()
                if j:
                    j = dirname(j)
                    if j:
                        p = basepath / j / p
                listed[l] = path(p)

    if (basedir / "chrome.manifest").isfile():
        parseCM()
    else:
        raise ValueError("Don't know what to do")

    if len(listed) == 0:
        raise ValueError("Not localized")
    if len(listed) == 1:
        raise ValueError("Only one locale")

    baseloc = None
    for bl in ('en-US', 'en', 'en-NZ', 'de-DE', 'de', 'fr-FR', 'fr'):
        if listed.has_key(bl):
            baseloc = bl
            break
    if not baseloc:
        raise ValueError("No base locale")

    print "Using", baseloc, "as base locale"

    bad = list()
    good = list()
    missing = list()
    for l,p in listed.iteritems():
        if not p.isdir():
          continue
        if l == baseloc:
            " skip base locale "
            continue

        listed[l] = True

        res = compareDirs(listed[baseloc], p)
        summary = res.getSummary()[None]
        if not 'missing' in summary and not 'errors' in summary:
            good += l,
            continue
        bad += l,
        if not 'changed' in summary:
            summary['changed'] = 0
        if not 'unchanged' in summary:
            summary['unchanged'] = 0
        summary["total"] = summary['changed'] + summary['unchanged']
        if not summary["total"]:
            summary["total"] = -1
        if not 'missing' in summary:
            summary['missing'] = 0

        if not 'errors' in summary:
            summary['errors'] = 0

        print "Locale: %s" % l
        print "Strings: %d (Missing: %d / %.2f%%)\nChanged: %d (%.2f%%), Unchanged %d (%.2f%%)\nErrors: %d" % (
            summary['total'],
            summary['missing'],
            100.0 * summary['missing'] / (summary['total'] + summary['missing']),
            summary['changed'],
            100.0 * summary['changed'] / summary['total'],
            summary['unchanged'],
            100.0 * summary['unchanged'] / summary['total'],
            summary['errors']
            )
        branches = list(res.details.branches)
        branches.sort()
        print "Issues:"
        for b in branches:
            for f in b:
                print "*", f
                for k, v in res.details[f].iteritems():
                    if isinstance(v, list):
                        v = ', '.join(v)
                    try:
                        print "\t%s: %s" % (unicode(k), unicode(v))
                    except:
                        pass
        print
    for l,p in listed.iteritems():
        if p is True or l == baseloc:
            continue
        missing += l,
    if bad:
        print "Bad:", ' '.join(bad)
    if missing:
        print "Missing:", ' '.join(missing)
    if good:
        print 'Good:', ' '.join(good)
    if bad or missing:
        raise ValueError("Not valid")

if __name__ == "__main__":
    import sys
    try:
        cmp_listed_locales(path("."))
    except Exception,ex:
        print >>sys.stderr, ex
        sys.exit(1)
    else:
        sys.exit(0)
