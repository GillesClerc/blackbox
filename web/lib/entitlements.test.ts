import { describe, expect, it } from "vitest";
import {
  buildSyncList,
  decidePackageAccess,
  type ScenarioWithVersion,
} from "./entitlements";

const pub = (version = 4, storage = "repo") => ({
  version,
  status: "published",
  storage,
  storage_path: "capitaine_verdier",
});

const verdier: ScenarioWithVersion = {
  id: "s1",
  slug: "capitaine_verdier",
  title: "Capitaine Verdier",
  status: "published",
  current_version: pub(4),
};
const brouillon: ScenarioWithVersion = {
  id: "s2",
  slug: "brouillon",
  title: "Brouillon",
  status: "draft",
  current_version: pub(1),
};
const archivee: ScenarioWithVersion = {
  id: "s3",
  slug: "archivee",
  title: "Archivée",
  status: "archived",
  current_version: pub(2),
};
const sansVersion: ScenarioWithVersion = {
  id: "s4",
  slug: "sans_version",
  title: "Sans version",
  status: "published",
  current_version: null,
};
const versionRetiree: ScenarioWithVersion = {
  id: "s5",
  slug: "retiree",
  title: "Version retirée",
  status: "published",
  current_version: { ...pub(3), status: "retired" },
};

const lic = (scenario_id: string) => ({ scenario_id, created_at: "2026-09-30T00:00:00Z" });

describe("buildSyncList", () => {
  it("renvoie le format attendu par le firmware (slug, package_path, version)", () => {
    expect(buildSyncList([lic("s1")], [verdier])).toEqual([
      {
        id: "s1",
        slug: "capitaine_verdier",
        title: "Capitaine Verdier",
        package_path: "/api/box/pkg/capitaine_verdier",
        version: 4,
        installed_at: "2026-09-30T00:00:00Z",
      },
    ]);
  });

  it("n'inclut que les histoires publiées avec une version publiée", () => {
    const all = [verdier, brouillon, archivee, sansVersion, versionRetiree];
    const out = buildSyncList(all.map((s) => lic(s.id)), all);
    expect(out.map((s) => s.slug)).toEqual(["capitaine_verdier"]);
  });

  it("n'inclut pas une histoire publiée sans licence", () => {
    expect(buildSyncList([], [verdier])).toEqual([]);
    expect(buildSyncList([lic("autre")], [verdier])).toEqual([]);
  });

  it("la version servie est celle de la version courante", () => {
    const v7 = { ...verdier, current_version: pub(7) };
    expect(buildSyncList([lic("s1")], [v7])[0].version).toBe(7);
  });

  it("trie par slug (réponse stable)", () => {
    const b = { ...verdier, id: "b", slug: "b_histoire" };
    const a = { ...verdier, id: "a", slug: "a_histoire" };
    expect(buildSyncList([lic("a"), lic("b")], [b, a]).map((s) => s.slug)).toEqual([
      "a_histoire",
      "b_histoire",
    ]);
  });
});

describe("decidePackageAccess", () => {
  it("404 si l'histoire n'existe pas ou n'est pas servable", () => {
    for (const s of [null, brouillon, archivee, sansVersion, versionRetiree]) {
      const r = decidePackageAccess(s, true);
      expect(r).toMatchObject({ ok: false, status: 404 });
    }
  });

  it("403 si l'histoire est publiée mais sans licence", () => {
    expect(decidePackageAccess(verdier, false)).toMatchObject({ ok: false, status: 403 });
  });

  it("accès à la version publiée avec une licence active", () => {
    expect(decidePackageAccess(verdier, true)).toEqual({
      ok: true,
      scenarioId: "s1",
      version: pub(4),
    });
  });
});
