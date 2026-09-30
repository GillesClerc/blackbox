import { afterEach, describe, expect, it } from "vitest";
import { siteBlockCrawl, siteNoIndex } from "./indexing";
import robots from "../app/robots";

afterEach(() => {
  delete process.env.SITE_NOINDEX;
  delete process.env.SITE_BLOCK_CRAWL;
});

describe("indexation", () => {
  it("par défaut : indexable, robots.txt ouvert hors espaces privés", () => {
    expect(siteNoIndex()).toBe(false);
    expect(siteBlockCrawl()).toBe(false);
    expect(robots().rules).toMatchObject({ allow: "/" });
  });

  it.each(["1", "true", "YES", " 1 "])("SITE_NOINDEX=« %s » : noindex, robots.txt reste ouvert", (v) => {
    process.env.SITE_NOINDEX = v;
    expect(siteNoIndex()).toBe(true);
    expect(robots().rules).toMatchObject({ allow: "/" });
  });

  it("SITE_BLOCK_CRAWL=1 : robots.txt interdit tout", () => {
    process.env.SITE_BLOCK_CRAWL = "1";
    expect(robots().rules).toEqual({ userAgent: "*", disallow: "/" });
  });

  it("« 0 », vide ou « false » ne changent rien", () => {
    for (const v of ["0", "", "false"]) {
      process.env.SITE_NOINDEX = v;
      process.env.SITE_BLOCK_CRAWL = v;
      expect(siteNoIndex()).toBe(false);
      expect(siteBlockCrawl()).toBe(false);
    }
  });
});
