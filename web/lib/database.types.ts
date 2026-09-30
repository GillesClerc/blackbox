// Généré depuis la base de staging (postgres-meta) : python3 tools/staging_sql.py --gen-types
// Ne pas modifier à la main — régénérer après chaque migration.

export type Json =
  | string
  | number
  | boolean
  | null
  | { [key: string]: Json | undefined }
  | Json[]

export type Database = {
  public: {
    Tables: {
      box_challenges: {
        Row: {
          box_uid: string
          challenge: string
          expires_at: string
          used: boolean
        }
        Insert: {
          box_uid: string
          challenge: string
          expires_at: string
          used?: boolean
        }
        Update: {
          box_uid?: string
          challenge?: string
          expires_at?: string
          used?: boolean
        }
        Relationships: []
      }
      device_scenarios: {
        Row: {
          device_id: string
          installed_at: string | null
          installed_version: number | null
          last_seen_at: string | null
          scenario_id: string
        }
        Insert: {
          device_id: string
          installed_at?: string | null
          installed_version?: number | null
          last_seen_at?: string | null
          scenario_id: string
        }
        Update: {
          device_id?: string
          installed_at?: string | null
          installed_version?: number | null
          last_seen_at?: string | null
          scenario_id?: string
        }
        Relationships: [
          {
            foreignKeyName: "device_scenarios_device_id_fkey"
            columns: ["device_id"]
            referencedRelation: "devices"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "device_scenarios_scenario_id_fkey"
            columns: ["scenario_id"]
            referencedRelation: "scenarios"
            referencedColumns: ["id"]
          },
        ]
      }
      devices: {
        Row: {
          box_uid: string
          created_at: string | null
          firmware_version: string | null
          id: string
          last_sync_at: string | null
          name: string | null
          owner_id: string | null
        }
        Insert: {
          box_uid: string
          created_at?: string | null
          firmware_version?: string | null
          id?: string
          last_sync_at?: string | null
          name?: string | null
          owner_id?: string | null
        }
        Update: {
          box_uid?: string
          created_at?: string | null
          firmware_version?: string | null
          id?: string
          last_sync_at?: string | null
          name?: string | null
          owner_id?: string | null
        }
        Relationships: [
          {
            foreignKeyName: "devices_owner_id_fkey"
            columns: ["owner_id"]
            referencedRelation: "profiles"
            referencedColumns: ["id"]
          },
        ]
      }
      firmware_releases: {
        Row: {
          active: boolean
          channel: string
          created_at: string | null
          id: string
          notes: string | null
          sha256: string
          url: string
          version: string
        }
        Insert: {
          active?: boolean
          channel?: string
          created_at?: string | null
          id?: string
          notes?: string | null
          sha256: string
          url: string
          version: string
        }
        Update: {
          active?: boolean
          channel?: string
          created_at?: string | null
          id?: string
          notes?: string | null
          sha256?: string
          url?: string
          version?: string
        }
        Relationships: []
      }
      licenses: {
        Row: {
          created_at: string
          granted_by: string | null
          id: string
          note: string | null
          revoke_reason: string | null
          revoked_at: string | null
          scenario_id: string
          source: string
          stripe_payment_intent: string | null
          user_id: string
        }
        Insert: {
          created_at?: string
          granted_by?: string | null
          id?: string
          note?: string | null
          revoke_reason?: string | null
          revoked_at?: string | null
          scenario_id: string
          source: string
          stripe_payment_intent?: string | null
          user_id: string
        }
        Update: {
          created_at?: string
          granted_by?: string | null
          id?: string
          note?: string | null
          revoke_reason?: string | null
          revoked_at?: string | null
          scenario_id?: string
          source?: string
          stripe_payment_intent?: string | null
          user_id?: string
        }
        Relationships: [
          {
            foreignKeyName: "licenses_granted_by_fkey"
            columns: ["granted_by"]
            referencedRelation: "profiles"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "licenses_scenario_id_fkey"
            columns: ["scenario_id"]
            referencedRelation: "scenarios"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "licenses_user_id_fkey"
            columns: ["user_id"]
            referencedRelation: "profiles"
            referencedColumns: ["id"]
          },
        ]
      }
      profiles: {
        Row: {
          created_at: string | null
          email: string | null
          id: string
          role: string
        }
        Insert: {
          created_at?: string | null
          email?: string | null
          id: string
          role?: string
        }
        Update: {
          created_at?: string | null
          email?: string | null
          id?: string
          role?: string
        }
        Relationships: []
      }
      scenario_versions: {
        Row: {
          created_at: string
          created_by: string | null
          id: string
          manifest: Json | null
          notes: string | null
          published_at: string | null
          scenario_id: string
          status: string
          storage: string
          storage_path: string
          total_bytes: number | null
          version: number
        }
        Insert: {
          created_at?: string
          created_by?: string | null
          id?: string
          manifest?: Json | null
          notes?: string | null
          published_at?: string | null
          scenario_id: string
          status?: string
          storage?: string
          storage_path: string
          total_bytes?: number | null
          version: number
        }
        Update: {
          created_at?: string
          created_by?: string | null
          id?: string
          manifest?: Json | null
          notes?: string | null
          published_at?: string | null
          scenario_id?: string
          status?: string
          storage?: string
          storage_path?: string
          total_bytes?: number | null
          version?: number
        }
        Relationships: [
          {
            foreignKeyName: "scenario_versions_created_by_fkey"
            columns: ["created_by"]
            referencedRelation: "profiles"
            referencedColumns: ["id"]
          },
          {
            foreignKeyName: "scenario_versions_scenario_id_fkey"
            columns: ["scenario_id"]
            referencedRelation: "scenarios"
            referencedColumns: ["id"]
          },
        ]
      }
      scenarios: {
        Row: {
          active: boolean
          ambiance: string | null
          cover_path: string | null
          created_at: string | null
          current_version_id: string | null
          description: string | null
          difficulty: number | null
          duration_min: number | null
          id: string
          language: string
          max_players: number | null
          min_age: number | null
          min_players: number | null
          package_path: string | null
          price_chf: number
          slug: string
          status: string
          summary: string | null
          theme: string | null
          title: string
          updated_at: string
          version: number
        }
        Insert: {
          active?: boolean
          ambiance?: string | null
          cover_path?: string | null
          created_at?: string | null
          current_version_id?: string | null
          description?: string | null
          difficulty?: number | null
          duration_min?: number | null
          id?: string
          language?: string
          max_players?: number | null
          min_age?: number | null
          min_players?: number | null
          package_path?: string | null
          price_chf?: number
          slug: string
          status?: string
          summary?: string | null
          theme?: string | null
          title: string
          updated_at?: string
          version?: number
        }
        Update: {
          active?: boolean
          ambiance?: string | null
          cover_path?: string | null
          created_at?: string | null
          current_version_id?: string | null
          description?: string | null
          difficulty?: number | null
          duration_min?: number | null
          id?: string
          language?: string
          max_players?: number | null
          min_age?: number | null
          min_players?: number | null
          package_path?: string | null
          price_chf?: number
          slug?: string
          status?: string
          summary?: string | null
          theme?: string | null
          title?: string
          updated_at?: string
          version?: number
        }
        Relationships: [
          {
            foreignKeyName: "scenarios_current_version_id_fkey"
            columns: ["current_version_id"]
            referencedRelation: "scenario_versions"
            referencedColumns: ["id"]
          },
        ]
      }
      waitlist: {
        Row: {
          created_at: string | null
          email: string
          id: string
        }
        Insert: {
          created_at?: string | null
          email: string
          id?: string
        }
        Update: {
          created_at?: string | null
          email?: string
          id?: string
        }
        Relationships: []
      }
    }
    Views: {
      [_ in never]: never
    }
    Functions: {
      publish_scenario_version: {
        Args: { p_version_id: string }
        Returns: undefined
      }
    }
    Enums: {
      [_ in never]: never
    }
    CompositeTypes: {
      [_ in never]: never
    }
  }
}

type DatabaseWithoutInternals = Omit<Database, "__InternalSupabase">

type DefaultSchema = DatabaseWithoutInternals[Extract<keyof Database, "public">]

export type Tables<
  DefaultSchemaTableNameOrOptions extends
    | keyof (DefaultSchema["Tables"] & DefaultSchema["Views"])
    | { schema: keyof DatabaseWithoutInternals },
  TableName extends DefaultSchemaTableNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof (DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"] &
        DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Views"])
    : never = never,
> = DefaultSchemaTableNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? (DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"] &
      DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Views"])[TableName] extends {
      Row: infer R
    }
    ? R
    : never
  : DefaultSchemaTableNameOrOptions extends keyof (DefaultSchema["Tables"] &
        DefaultSchema["Views"])
    ? (DefaultSchema["Tables"] &
        DefaultSchema["Views"])[DefaultSchemaTableNameOrOptions] extends {
        Row: infer R
      }
      ? R
      : never
    : never

export type TablesInsert<
  DefaultSchemaTableNameOrOptions extends
    | keyof DefaultSchema["Tables"]
    | { schema: keyof DatabaseWithoutInternals },
  TableName extends DefaultSchemaTableNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"]
    : never = never,
> = DefaultSchemaTableNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"][TableName] extends {
      Insert: infer I
    }
    ? I
    : never
  : DefaultSchemaTableNameOrOptions extends keyof DefaultSchema["Tables"]
    ? DefaultSchema["Tables"][DefaultSchemaTableNameOrOptions] extends {
        Insert: infer I
      }
      ? I
      : never
    : never

export type TablesUpdate<
  DefaultSchemaTableNameOrOptions extends
    | keyof DefaultSchema["Tables"]
    | { schema: keyof DatabaseWithoutInternals },
  TableName extends DefaultSchemaTableNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"]
    : never = never,
> = DefaultSchemaTableNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[DefaultSchemaTableNameOrOptions["schema"]]["Tables"][TableName] extends {
      Update: infer U
    }
    ? U
    : never
  : DefaultSchemaTableNameOrOptions extends keyof DefaultSchema["Tables"]
    ? DefaultSchema["Tables"][DefaultSchemaTableNameOrOptions] extends {
        Update: infer U
      }
      ? U
      : never
    : never

export type Enums<
  DefaultSchemaEnumNameOrOptions extends
    | keyof DefaultSchema["Enums"]
    | { schema: keyof DatabaseWithoutInternals },
  EnumName extends DefaultSchemaEnumNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[DefaultSchemaEnumNameOrOptions["schema"]]["Enums"]
    : never = never,
> = DefaultSchemaEnumNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[DefaultSchemaEnumNameOrOptions["schema"]]["Enums"][EnumName]
  : DefaultSchemaEnumNameOrOptions extends keyof DefaultSchema["Enums"]
    ? DefaultSchema["Enums"][DefaultSchemaEnumNameOrOptions]
    : never

export type CompositeTypes<
  PublicCompositeTypeNameOrOptions extends
    | keyof DefaultSchema["CompositeTypes"]
    | { schema: keyof DatabaseWithoutInternals },
  CompositeTypeName extends PublicCompositeTypeNameOrOptions extends {
    schema: keyof DatabaseWithoutInternals
  }
    ? keyof DatabaseWithoutInternals[PublicCompositeTypeNameOrOptions["schema"]]["CompositeTypes"]
    : never = never,
> = PublicCompositeTypeNameOrOptions extends {
  schema: keyof DatabaseWithoutInternals
}
  ? DatabaseWithoutInternals[PublicCompositeTypeNameOrOptions["schema"]]["CompositeTypes"][CompositeTypeName]
  : PublicCompositeTypeNameOrOptions extends keyof DefaultSchema["CompositeTypes"]
    ? DefaultSchema["CompositeTypes"][PublicCompositeTypeNameOrOptions]
    : never

export const Constants = {
  public: {
    Enums: {},
  },
} as const
