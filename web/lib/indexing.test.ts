import { afterEach, describe, expect, it } from "vitest";
import { siteNoIndex } from "./indexing";
import robots from "../app/robots";

afterEach(() => {
  delete process.env.SITE_NOINDEX;
});

describe("SITE_NOINDEX", () => {
  it("indexable par défaut, espaces privés exclus", () => {
    expect(siteNoIndex()).toBe(false);
    const r = robots();
    expect(r.rules).toMatchObject({ allow: "/" });
  });

  it.each(["1", "true", "YES", " 1 "])("« %s » désindexe tout le site", (v) => {
    process.env.SITE_NOINDEX = v;
    expect(siteNoIndex()).toBe(true);
    expect(robots().rules).toEqual({ userAgent: "*", disallow: "/" });
  });

  it("« 0 » ou vide ne désindexe pas", () => {
    for (const v of ["0", "", "false"]) {
      process.env.SITE_NOINDEX = v;
      expect(siteNoIndex()).toBe(false);
    }
  });
});
