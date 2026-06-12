"use server";

import { redirect } from "next/navigation";
import { z } from "zod";
import { createClient } from "@/lib/supabase/server";

const credentialsSchema = z.object({
  email: z.email(),
  password: z.string().min(8),
});

export type AuthState = {
  status: "idle" | "error";
  message?: string;
};

function parseCredentials(formData: FormData) {
  return credentialsSchema.safeParse({
    email: String(formData.get("email") ?? "")
      .trim()
      .toLowerCase(),
    password: String(formData.get("password") ?? ""),
  });
}

export async function signUp(
  _prev: AuthState,
  formData: FormData
): Promise<AuthState> {
  const parsed = parseCredentials(formData);
  if (!parsed.success) {
    return {
      status: "error",
      message:
        "Vérifiez l'adresse e-mail, et choisissez un mot de passe d'au moins 8 caractères.",
    };
  }

  const supabase = await createClient();
  const { error } = await supabase.auth.signUp(parsed.data);

  if (error) {
    if (error.code === "user_already_exists") {
      return {
        status: "error",
        message: "Un compte existe déjà avec cette adresse. Connectez-vous.",
      };
    }
    return {
      status: "error",
      message: "La création du compte a échoué. Réessayez dans un instant.",
    };
  }

  redirect("/account");
}

export async function signIn(
  _prev: AuthState,
  formData: FormData
): Promise<AuthState> {
  const parsed = parseCredentials(formData);
  if (!parsed.success) {
    return {
      status: "error",
      message: "Vérifiez l'adresse e-mail et le mot de passe.",
    };
  }

  const supabase = await createClient();
  const { error } = await supabase.auth.signInWithPassword(parsed.data);

  if (error) {
    return {
      status: "error",
      message: "Adresse ou mot de passe incorrect.",
    };
  }

  redirect("/account");
}

export async function signOut() {
  const supabase = await createClient();
  await supabase.auth.signOut();
  redirect("/");
}
