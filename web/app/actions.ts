"use server";

import { z } from "zod";
import { createClient } from "@/lib/supabase/server";

const schema = z.object({
  email: z.email(),
});

export type WaitlistState = {
  status: "idle" | "ok" | "error";
  message?: string;
};

export async function joinWaitlist(
  _prev: WaitlistState,
  formData: FormData
): Promise<WaitlistState> {
  const parsed = schema.safeParse({
    email: String(formData.get("email") ?? "")
      .trim()
      .toLowerCase(),
  });
  if (!parsed.success) {
    return {
      status: "error",
      message: "Cette adresse e-mail ne semble pas valide.",
    };
  }

  if (
    !process.env.NEXT_PUBLIC_SUPABASE_URL ||
    !process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY
  ) {
    return {
      status: "error",
      message: "L'inscription est momentanément indisponible. Réessayez plus tard.",
    };
  }

  const supabase = await createClient();
  const { error } = await supabase
    .from("waitlist")
    .insert({ email: parsed.data.email });

  // 23505 : e-mail déjà inscrit → succès idempotent, pas d'énumération d'adresses
  if (error && error.code !== "23505") {
    return {
      status: "error",
      message: "L'inscription a échoué. Réessayez dans un instant.",
    };
  }

  return { status: "ok" };
}
